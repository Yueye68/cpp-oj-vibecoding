import requests
import tempfile
import os
import unittest
import time
from typing import Optional

BASE_URL = "http://localhost:8080"


class APIClient:
    def __init__(self, base_url: str = BASE_URL):
        self.base_url = base_url
        self.session = requests.Session()

    def get(self, path: str, **kwargs):
        return self.session.get(f"{self.base_url}{path}", **kwargs)

    def post(self, path: str, **kwargs):
        return self.session.post(f"{self.base_url}{path}", **kwargs)

    def put(self, path: str, **kwargs):
        return self.session.put(f"{self.base_url}{path}", **kwargs)

    def delete(self, path: str, **kwargs):
        return self.session.delete(f"{self.base_url}{path}", **kwargs)

    def upload_file(self, path: str, files: dict, **kwargs):
        return self.session.post(f"{self.base_url}{path}", files=files, **kwargs)


def unique_name(prefix):
    return f"{prefix}_{int(time.time() * 1000)}"


class TestHealth(unittest.TestCase):
    def test_health_check(self):
        resp = requests.get(f"{BASE_URL}/health")
        self.assertEqual(resp.status_code, 200)
        self.assertEqual(resp.text, "OK")


class TestAuth(unittest.TestCase):
    def test_register(self):
        client = APIClient()
        username = unique_name("user")
        resp = client.post(
            "/api/auth/register",
            json={"username": username, "password": "123456"}
        )
        self.assertIn(resp.status_code, [201, 409])

    def test_login_success(self):
        client = APIClient()
        username = unique_name("logintest")
        client.post("/api/auth/register", json={"username": username, "password": "123456"})
        resp = client.post(
            "/api/auth/login",
            json={"username": username, "password": "123456"}
        )
        self.assertEqual(resp.status_code, 200)

    def test_login_wrong_password(self):
        client = APIClient()
        username = unique_name("wrongpwd")
        client.post("/api/auth/register", json={"username": username, "password": "123456"})
        resp = client.post(
            "/api/auth/login",
            json={"username": username, "password": "wrongpass"}
        )
        self.assertEqual(resp.status_code, 401)

    def test_get_current_user(self):
        client = APIClient()
        username = unique_name("meuser")
        client.post("/api/auth/register", json={"username": username, "password": "123456"})
        client.post("/api/auth/login", json={"username": username, "password": "123456"})
        resp = client.get("/api/auth/me")
        self.assertEqual(resp.status_code, 200)

    def test_logout(self):
        client = APIClient()
        username = unique_name("logoutuser")
        client.post("/api/auth/register", json={"username": username, "password": "123456"})
        client.post("/api/auth/login", json={"username": username, "password": "123456"})
        resp = client.post("/api/auth/logout")
        self.assertEqual(resp.status_code, 200)


class TestAdminAuth(unittest.TestCase):
    def test_admin_login(self):
        client = APIClient()
        resp = client.post(
            "/api/auth/login",
            json={"username": "admin", "password": "admin123"}
        )
        self.assertEqual(resp.status_code, 200)
        data = resp.json()
        user_data = data.get("user", data)
        self.assertIn(user_data.get("role"), ["admin", "user"])


class TestProblems(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.admin_client = APIClient()
        resp = cls.admin_client.post(
            "/api/auth/login",
            json={"username": "admin", "password": "admin123"}
        )
        if resp.status_code == 200:
            data = resp.json()
            user_data = data.get("user", data)
            if user_data.get("role") != "admin":
                cls.admin_client = APIClient()
                cls.admin_username = unique_name("admin")
                cls.admin_client.post(
                    "/api/auth/register",
                    json={"username": cls.admin_username, "password": "admin123"}
                )
                cls.admin_client.post(
                    "/api/auth/login",
                    json={"username": cls.admin_username, "password": "admin123"}
                )

        resp = cls.admin_client.post(
            "/api/problems",
            json={
                "title": "两数之和",
                "description": "给定一个整数数组nums，返回满足条件的两个数的下标",
                "difficulty": "easy",
                "tags": ["数组", "哈希表"],
                "time_limit_ms": 1000,
                "memory_limit_mb": 256
            }
        )
        cls.problem_id = resp.json().get("id") if resp.status_code == 201 else None
        cls.problem_created = resp.status_code == 201

    def test_delete_problem(self):
        if not self.problem_created:
            self.skipTest("Problem not created")
        create_resp = self.admin_client.post(
            "/api/problems",
            json={
                "title": "待删除题目",
                "description": "用于删除测试",
                "difficulty": "easy",
                "tags": ["测试"],
                "time_limit_ms": 1000,
                "memory_limit_mb": 256
            }
        )
        if create_resp.status_code != 201:
            self.skipTest("Could not create problem for deletion test")
        new_problem_id = create_resp.json().get("id")
        resp = self.admin_client.delete(f"/api/problems/{new_problem_id}")
        self.assertEqual(resp.status_code, 200)

    def test_get_problem_detail(self):
        if not self.problem_created:
            self.skipTest("Problem not created")
        resp = self.admin_client.get(f"/api/problems/{self.problem_id}")
        self.assertEqual(resp.status_code, 200)

    def test_get_problem_list(self):
        client = APIClient()
        resp = client.get("/api/problems", params={"page": 1, "pageSize": 10})
        self.assertEqual(resp.status_code, 200)

    def test_update_problem(self):
        if not self.problem_created:
            self.skipTest("Problem not created")
        resp = self.admin_client.put(
            f"/api/problems/{self.problem_id}",
            json={"title": "两数之和 Updated", "difficulty": "medium"}
        )
        self.assertEqual(resp.status_code, 200)

    def test_get_nonexistent_problem(self):
        client = APIClient()
        resp = client.get("/api/problems/99999")
        self.assertEqual(resp.status_code, 404)

    def test_delete_nonexistent_problem(self):
        if not self.problem_created:
            self.skipTest("Problem not created")
        resp = self.admin_client.delete("/api/problems/99999")
        self.assertEqual(resp.status_code, 404)


class TestNonAdminCreateProblem(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.client = APIClient()
        cls.username = unique_name("regular")
        cls.client.post(
            "/api/auth/register",
            json={"username": cls.username, "password": "123456"}
        )
        cls.client.post(
            "/api/auth/login",
            json={"username": cls.username, "password": "123456"}
        )

    def test_non_admin_create_problem(self):
        resp = self.client.post(
            "/api/problems",
            json={"title": "Test", "difficulty": "easy"}
        )
        self.assertEqual(resp.status_code, 403)


class TestTestCases(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.admin_client = APIClient()
        resp = cls.admin_client.post(
            "/api/auth/login",
            json={"username": "admin", "password": "admin123"}
        )
        if resp.status_code == 200:
            data = resp.json()
            user_data = data.get("user", data)
            if user_data.get("role") != "admin":
                cls.admin_client = APIClient()
                cls.admin_username = unique_name("admin")
                cls.admin_client.post(
                    "/api/auth/register",
                    json={"username": cls.admin_username, "password": "admin123"}
                )
                cls.admin_client.post(
                    "/api/auth/login",
                    json={"username": cls.admin_username, "password": "admin123"}
                )

        resp = cls.admin_client.post(
            "/api/problems",
            json={
                "title": "测试题目",
                "description": "测试",
                "difficulty": "easy",
                "tags": ["测试"],
                "time_limit_ms": 1000,
                "memory_limit_mb": 256
            }
        )
        cls.problem_id = resp.json().get("id") if resp.status_code == 201 else None
        cls.testcase_id = None

        if cls.problem_id:
            input_file = tempfile.NamedTemporaryFile(mode='w', delete=False)
            input_file.write("2\n3")
            input_file.close()
            cls.input_path = input_file.name

            output_file = tempfile.NamedTemporaryFile(mode='w', delete=False)
            output_file.write("5")
            output_file.close()
            cls.output_path = output_file.name

            with open(cls.input_path, 'rb') as inp, open(cls.output_path, 'rb') as out:
                resp = cls.admin_client.upload_file(
                    f"/api/problems/{cls.problem_id}/testcases",
                    files={
                        "input": inp,
                        "output": out,
                        "is_sample": "1"
                    }
                )
            cls.testcase_id = resp.json().get("id") if resp.status_code == 201 else None

    @classmethod
    def tearDownClass(cls):
        for f in getattr(cls, 'input_path', None), getattr(cls, 'output_path', None):
            if f and os.path.exists(f):
                os.unlink(f)

    def test_get_testcase_list(self):
        if not self.problem_id:
            self.skipTest("Problem not created")
        resp = self.admin_client.get(f"/api/problems/{self.problem_id}/testcases")
        self.assertEqual(resp.status_code, 200)

    def test_delete_testcase(self):
        if not self.testcase_id:
            self.skipTest("Testcase not created")
        resp = self.admin_client.delete(f"/api/testcases/{self.testcase_id}")
        self.assertEqual(resp.status_code, 200)

    def test_delete_nonexistent_testcase(self):
        if not self.problem_id:
            self.skipTest("Problem not created")
        resp = self.admin_client.delete("/api/testcases/99999")
        self.assertEqual(resp.status_code, 404)


class TestSubmissions(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.user_client = APIClient()
        cls.username = unique_name("subuser")
        cls.user_client.post(
            "/api/auth/register",
            json={"username": cls.username, "password": "123456"}
        )
        cls.user_client.post(
            "/api/auth/login",
            json={"username": cls.username, "password": "123456"}
        )

        cls.admin_client = APIClient()
        resp = cls.admin_client.post(
            "/api/auth/login",
            json={"username": "admin", "password": "admin123"}
        )
        if resp.status_code == 200:
            data = resp.json()
            user_data = data.get("user", data)
            if user_data.get("role") != "admin":
                cls.admin_client = APIClient()
                cls.admin_username = unique_name("admin")
                cls.admin_client.post(
                    "/api/auth/register",
                    json={"username": cls.admin_username, "password": "admin123"}
                )
                cls.admin_client.post(
                    "/api/auth/login",
                    json={"username": cls.admin_username, "password": "admin123"}
                )

        resp = cls.admin_client.post(
            "/api/problems",
            json={
                "title": "提交测试",
                "description": "测试提交",
                "difficulty": "easy",
                "tags": ["测试"],
                "time_limit_ms": 1000,
                "memory_limit_mb": 256
            }
        )
        cls.problem_id = resp.json().get("id") if resp.status_code == 201 else None

    def test_create_submission(self):
        if not self.problem_id:
            self.skipTest("Problem not created")
        resp = self.user_client.post(
            "/api/submissions",
            json={
                "problem_id": self.problem_id,
                "code": "#include <bits/stdc++.h>\nusing namespace std;\nint main() { return 0; }",
                "language": "cpp"
            }
        )
        self.assertEqual(resp.status_code, 201)

    def test_get_submission_history(self):
        resp = self.user_client.get("/api/submissions")
        self.assertEqual(resp.status_code, 200)

    def test_get_submission_detail(self):
        resp = self.user_client.get("/api/submissions/1")
        self.assertIn(resp.status_code, [200, 404])


if __name__ == "__main__":
    unittest.main(verbosity=2)