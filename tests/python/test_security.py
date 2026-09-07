import requests
import tempfile
import os
import unittest
import time

BASE_URL = "http://localhost:8080"


class APIClient:
    def __init__(self):
        self.session = requests.Session()

    def post(self, path, **kwargs):
        return self.session.post(f"{BASE_URL}{path}", **kwargs)

    def get(self, path, **kwargs):
        return self.session.get(f"{BASE_URL}{path}", **kwargs)

    def put(self, path, **kwargs):
        return self.session.put(f"{BASE_URL}{path}", **kwargs)

    def delete(self, path, **kwargs):
        return self.session.delete(f"{BASE_URL}{path}", **kwargs)

    def upload_file(self, path, files, **kwargs):
        return self.session.post(f"{BASE_URL}{path}", files=files, **kwargs)


class TestPermissionControl(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.user_client = APIClient()
        resp = cls.user_client.post("/api/auth/register", json={"username": "security_user", "password": "123456"})
        cls.user_client.post("/api/auth/login", json={"username": "security_user", "password": "123456"})

    def test_user_cannot_create_problem(self):
        resp = self.user_client.post("/api/problems", json={"title": "Test", "difficulty": "easy"})
        self.assertEqual(resp.status_code, 403)

    def test_user_cannot_delete_problem(self):
        resp = self.user_client.delete("/api/problems/1")
        self.assertEqual(resp.status_code, 403)

    def test_user_cannot_upload_testcase(self):
        resp = self.user_client.post("/api/problems/1/testcases")
        self.assertEqual(resp.status_code, 403)

    def test_user_cannot_delete_testcase(self):
        resp = self.user_client.delete("/api/testcases/1")
        self.assertEqual(resp.status_code, 403)


class TestTestcaseIsolation(unittest.TestCase):
    def test_cannot_access_testcase_files(self):
        resp = requests.get(f"{BASE_URL}/uploads/test_cases/1/input.txt")
        self.assertIn(resp.status_code, [403, 404])

    def test_cannot_access_uploads_dir(self):
        resp = requests.get(f"{BASE_URL}/uploads/")
        self.assertIn(resp.status_code, [403, 404])


class TestCodeExecutionIsolation(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.user_client = APIClient()
        resp = cls.user_client.post("/api/auth/register", json={"username": "code_user", "password": "123456"})
        cls.user_client.post("/api/auth/login", json={"username": "code_user", "password": "123456"})

        admin = APIClient()
        admin.post("/api/auth/login", json={"username": "admin", "password": "admin123"})
        resp = admin.post("/api/problems", json={"title": "安全测试", "difficulty": "easy", "tags": ["测试"], "time_limit_ms": 5000, "memory_limit_mb": 256})
        cls.problem_id = resp.json().get("id")

        inp = tempfile.NamedTemporaryFile(mode='w', delete=False)
        inp.write("1")
        inp.close()
        out = tempfile.NamedTemporaryFile(mode='w', delete=False)
        out.write("1")
        out.close()
        cls._temp_files = [inp.name, out.name]

        with open(inp.name, 'rb') as f_in, open(out.name, 'rb') as f_out:
            admin.upload_file(f"/api/problems/{cls.problem_id}/testcases", files={"input": f_in, "output": f_out, "is_sample": "1"})

    @classmethod
    def tearDownClass(cls):
        for f in getattr(cls, '_temp_files', []):
            if f and os.path.exists(f):
                os.unlink(f)

    def poll_submission(self, sub_id, timeout=30):
        for _ in range(timeout):
            r = self.user_client.get(f"/api/submissions/{sub_id}")
            if r.json().get("queue_status") in ["completed", "failed"]:
                return r.json()
            time.sleep(1)
        return None

    def test_fork_bomb_isolation(self):
        code = """#include <bits/stdc++.h>
int main() {
    while(fork()) {}
    return 0;
}"""
        resp = self.user_client.post("/api/submissions", json={"problem_id": self.problem_id, "code": code, "language": "cpp"})
        self.assertEqual(resp.status_code, 201)
        sub_id = resp.json()["id"]

        result = self.poll_submission(sub_id, timeout=30)
        self.assertIsNotNone(result)
        self.assertIn(result.get("status"), ["RE", "TLE"])

    def test_infinite_loop_isolation(self):
        code = """#include <bits/stdc++.h>
int main() {
    while(true) {}
    return 0;
}"""
        resp = self.user_client.post("/api/submissions", json={"problem_id": self.problem_id, "code": code, "language": "cpp"})
        self.assertEqual(resp.status_code, 201)
        sub_id = resp.json()["id"]

        result = self.poll_submission(sub_id, timeout=30)
        self.assertIsNotNone(result)
        self.assertEqual(result.get("status"), "TLE")

    def test_memory_limit_isolation(self):
        code = """#include <bits/stdc++.h>
int main() {
    std::vector<int> v(1000000000, 1);
    return 0;
}"""
        resp = self.user_client.post("/api/submissions", json={"problem_id": self.problem_id, "code": code, "language": "cpp"})
        self.assertEqual(resp.status_code, 201)
        sub_id = resp.json()["id"]

        result = self.poll_submission(sub_id, timeout=30)
        self.assertIsNotNone(result)
        self.assertIn(result.get("status"), ["MLE", "RE"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
