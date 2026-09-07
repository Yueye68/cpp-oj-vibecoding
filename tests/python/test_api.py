import requests
import tempfile
import os
import unittest
import time
from typing import Optional, Tuple

BASE_URL = "http://localhost:8080"

DEFAULT_POLL_INTERVAL = 1
DEFAULT_POLL_TIMEOUT = 30


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

    def poll_submission(self, submission_id: int, interval: int = DEFAULT_POLL_INTERVAL,
                        timeout: int = DEFAULT_POLL_TIMEOUT) -> Tuple[dict, str]:
        elapsed = 0
        while elapsed < timeout:
            resp = self.get(f"/api/submissions/{submission_id}")
            if resp.status_code != 200:
                return {}, "error"
            data = resp.json()
            queue_status = data.get("queue_status", "unknown")
            if queue_status in ["completed", "failed"]:
                return data, queue_status
            time.sleep(interval)
            elapsed += interval
        return {}, "timeout"


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

        if cls.problem_id:
            input_file = tempfile.NamedTemporaryFile(mode='w', delete=False)
            input_file.write("1 2 3")
            input_file.close()
            cls.input_path = input_file.name

            output_file = tempfile.NamedTemporaryFile(mode='w', delete=False)
            output_file.write("6")
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
        data = resp.json()
        self.assertEqual(data.get("status"), "pending")
        self.assertEqual(data.get("queue_status"), "pending")
        self.assertIn("id", data)

    def test_create_submission_with_polling(self):
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
        submission_id = resp.json().get("id")
        self.assertIsNotNone(submission_id)

        result_data, queue_status = self.user_client.poll_submission(submission_id)
        self.assertIn(queue_status, ["completed", "failed"])
        if queue_status == "completed":
            self.assertIn(result_data.get("status"), ["AC", "WA", "CE", "TLE", "MLE", "RE", "PE"])

    def test_get_submission_history(self):
        resp = self.user_client.get("/api/submissions")
        self.assertEqual(resp.status_code, 200)
        data = resp.json()
        submissions = data.get("submissions", [])
        for sub in submissions:
            self.assertIn("queue_status", sub)

    def test_get_submission_detail(self):
        resp = self.user_client.get("/api/submissions/1")
        self.assertIn(resp.status_code, [200, 404])


class TestEndToEnd(unittest.TestCase):
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
                cls.admin_username = unique_name("e2e_admin")
                cls.admin_client.post(
                    "/api/auth/register",
                    json={"username": cls.admin_username, "password": "admin123"}
                )
                cls.admin_client.post(
                    "/api/auth/login",
                    json={"username": cls.admin_username, "password": "admin123"}
                )

        cls.user_client = APIClient()
        cls.username = unique_name("e2e_user")
        cls.user_client.post(
            "/api/auth/register",
            json={"username": cls.username, "password": "123456"}
        )
        cls.user_client.post(
            "/api/auth/login",
            json={"username": cls.username, "password": "123456"}
        )

        resp = cls.admin_client.post(
            "/api/problems",
            json={
                "title": "两数之和-完整测试",
                "description": "给定一个整数数组 nums 和一个目标值 target，请返回满足 nums[i] + nums[j] = target 的两个数的下标。\n\n**示例**\n输入: nums = [2,7,11,15], target = 9\n输出: [0,1]\n\n**题目来源**: LeetCode 1",
                "difficulty": "easy",
                "tags": ["数组", "哈希表"],
                "time_limit_ms": 1000,
                "memory_limit_mb": 256
            }
        )
        cls.problem_id = resp.json().get("id") if resp.status_code == 201 else None

        if cls.problem_id:
            test_cases = [
                ("4\n2 7 11 15\n9", "0 1"),
                ("3\n3 2 4\n6", "0 2"),
                ("2\n3 3\n6", "0 1"),
            ]
            cls.testcase_ids = []
            for i, (inp, out) in enumerate(test_cases):
                input_file = tempfile.NamedTemporaryFile(mode='w', delete=False)
                input_file.write(inp)
                input_file.close()

                output_file = tempfile.NamedTemporaryFile(mode='w', delete=False)
                output_file.write(out)
                output_file.close()

                with open(input_file.name, 'rb') as f_in, open(output_file.name, 'rb') as f_out:
                    resp = cls.admin_client.upload_file(
                        f"/api/problems/{cls.problem_id}/testcases",
                        files={
                            "input": f_in,
                            "output": f_out,
                            "is_sample": "1" if i == 0 else "0"
                        }
                    )
                os.unlink(input_file.name)
                os.unlink(output_file.name)
                if resp.status_code == 201:
                    cls.testcase_ids.append(resp.json().get("id"))

    def test_e2e_problem_flow(self):
        if not self.problem_id:
            self.skipTest("Problem not created")
        resp = self.admin_client.get(f"/api/problems/{self.problem_id}")
        self.assertEqual(resp.status_code, 200)
        data = resp.json()
        self.assertEqual(data.get("title"), "两数之和-完整测试")
        self.assertEqual(data.get("difficulty"), "easy")
        self.assertIn("哈希表", data.get("tags", []))

    def test_e2e_submission_ac(self):
        if not self.problem_id:
            self.skipTest("Problem not created")
        code = """#include <bits/stdc++.h>
using namespace std;

class Solution {
public:
    vector<int> twoSum(vector<int>& nums, int target) {
        unordered_map<int, int> mp;
        for (int i = 0; i < nums.size(); ++i) {
            int complement = target - nums[i];
            if (mp.find(complement) != mp.end()) {
                return {mp[complement], i};
            }
            mp[nums[i]] = i;
        }
        return {};
    }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    
    int n;
    vector<int> nums;
    int target;
    
    if (!(cin >> n)) return 0;
    nums.resize(n);
    for (int i = 0; i < n; ++i) cin >> nums[i];
    cin >> target;
    
    Solution sol;
    vector<int> result = sol.twoSum(nums, target);
    
    if (result.size() == 2) {
        cout << result[0] << " " << result[1] << endl;
    }
    
    return 0;
}"""

        resp = self.user_client.post(
            "/api/submissions",
            json={"problem_id": self.problem_id, "code": code, "language": "cpp"}
        )
        self.assertEqual(resp.status_code, 201)
        submission_id = resp.json().get("id")

        result_data, queue_status = self.user_client.poll_submission(submission_id, timeout=60)
        self.assertEqual(queue_status, "completed")
        self.assertEqual(result_data.get("status"), "AC")
        self.assertIsNotNone(result_data.get("execute_time_ms"))

    def test_e2e_submission_wa(self):
        if not self.problem_id:
            self.skipTest("Problem not created")
        code = """#include <bits/stdc++.h>
using namespace std;
int main() {
    cout << "0 0" << endl;
    return 0;
}"""

        resp = self.user_client.post(
            "/api/submissions",
            json={"problem_id": self.problem_id, "code": code, "language": "cpp"}
        )
        self.assertEqual(resp.status_code, 201)
        submission_id = resp.json().get("id")

        result_data, queue_status = self.user_client.poll_submission(submission_id, timeout=60)
        self.assertEqual(queue_status, "completed")
        self.assertEqual(result_data.get("status"), "WA")

    def test_e2e_submission_ce(self):
        if not self.problem_id:
            self.skipTest("Problem not created")
        code = """#include <bits/stdc++.h>
using namespace std;
int main() {
    undefined_function();
    return 0;
}"""

        resp = self.user_client.post(
            "/api/submissions",
            json={"problem_id": self.problem_id, "code": code, "language": "cpp"}
        )
        self.assertEqual(resp.status_code, 201)
        submission_id = resp.json().get("id")

        result_data, queue_status = self.user_client.poll_submission(submission_id, timeout=60)
        self.assertEqual(queue_status, "completed")
        self.assertEqual(result_data.get("status"), "CE")
        self.assertIsNotNone(result_data.get("error_detail"))

    def test_e2e_problem_list_pagination(self):
        resp = self.user_client.get("/api/problems", params={"page": 1, "pageSize": 5})
        self.assertEqual(resp.status_code, 200)
        data = resp.json()
        self.assertIn("problems", data)
        self.assertIn("total", data)
        self.assertLessEqual(len(data.get("problems", [])), 5)

    def test_e2e_problem_filter_difficulty(self):
        resp = self.user_client.get("/api/problems", params={"difficulty": "easy"})
        self.assertEqual(resp.status_code, 200)
        data = resp.json()
        for p in data.get("problems", []):
            self.assertEqual(p.get("difficulty"), "easy")

    def test_e2e_submission_detail_results(self):
        if not self.problem_id:
            self.skipTest("Problem not created")

        code = """#include <bits/stdc++.h>
using namespace std;
int main() { return 0; }"""

        resp = self.user_client.post(
            "/api/submissions",
            json={"problem_id": self.problem_id, "code": code, "language": "cpp"}
        )
        self.assertEqual(resp.status_code, 201)
        submission_id = resp.json().get("id")

        result_data, queue_status = self.user_client.poll_submission(submission_id, timeout=60)
        self.assertEqual(queue_status, "completed")

        detail_resp = self.user_client.get(f"/api/submissions/{submission_id}")
        self.assertEqual(detail_resp.status_code, 200)
        detail_data = detail_resp.json()
        self.assertIn("results", detail_data)


if __name__ == "__main__":
    unittest.main(verbosity=2)