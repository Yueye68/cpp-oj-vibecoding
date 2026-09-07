import requests
import tempfile
import os
import time
import unittest
from concurrent.futures import ThreadPoolExecutor, as_completed
from threading import Lock

BASE_URL = "http://localhost:8080"


class APIClient:
    def __init__(self):
        self.session = requests.Session()

    def post(self, path, **kwargs):
        return self.session.post(f"{BASE_URL}{path}", **kwargs)

    def get(self, path, **kwargs):
        return self.session.get(f"{BASE_URL}{path}", **kwargs)

    def upload_file(self, path, files, **kwargs):
        return self.session.post(f"{BASE_URL}{path}", files=files, **kwargs)


class TestPerformance(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.admin_client = APIClient()
        resp = cls.admin_client.post("/api/auth/login", json={"username": "admin", "password": "admin123"})
        if resp.status_code != 200 or resp.json().get("user", {}).get("role") != "admin":
            cls.admin_client = APIClient()
            import time
            username = f"perf_admin_{int(time.time())}"
            cls.admin_client.post("/api/auth/register", json={"username": username, "password": "admin123"})
            cls.admin_client.post("/api/auth/login", json={"username": username, "password": "admin123"})

        resp = cls.admin_client.post(
            "/api/problems",
            json={
                "title": "性能测试题目",
                "description": "用于性能测试",
                "difficulty": "easy",
                "tags": ["性能测试"],
                "time_limit_ms": 5000,
                "memory_limit_mb": 256
            }
        )
        cls.problem_id = resp.json().get("id") if resp.status_code == 201 else None

        if cls.problem_id:
            input_file = tempfile.NamedTemporaryFile(mode='w', delete=False)
            input_file.write("1 2 3")
            input_file.close()
            output_file = tempfile.NamedTemporaryFile(mode='w', delete=False)
            output_file.write("6")
            output_file.close()
            cls._temp_files = [input_file.name, output_file.name]

            with open(input_file.name, 'rb') as f_in, open(output_file.name, 'rb') as f_out:
                cls.admin_client.upload_file(
                    f"/api/problems/{cls.problem_id}/testcases",
                    files={"input": f_in, "output": f_out, "is_sample": "1"}
                )

    @classmethod
    def tearDownClass(cls):
        for f in getattr(cls, '_temp_files', []):
            if f and os.path.exists(f):
                os.unlink(f)

    def test_page_load_time(self):
        pages = ["/", "/problem_list.html", "/login.html", "/register.html"]
        for page in pages:
            start = time.time()
            resp = requests.get(f"{BASE_URL}{page}", timeout=10)
            elapsed = time.time() - start
            self.assertEqual(resp.status_code, 200)
            self.assertLess(elapsed, 1.0, f"{page} load time {elapsed:.3f}s > 1s")

    def test_api_response_time(self):
        endpoints = [
            ("/api/problems", {"params": {"page": 1, "pageSize": 20}}),
            ("/api/problems", {"params": {"difficulty": "easy"}}),
        ]
        for path, kwargs in endpoints:
            start = time.time()
            resp = requests.get(f"{BASE_URL}{path}", timeout=10, **kwargs)
            elapsed = time.time() - start
            self.assertEqual(resp.status_code, 200)
            self.assertLess(elapsed, 1.0, f"{path} response time {elapsed:.3f}s > 1s")

    def test_concurrent_submissions(self):
        if not self.problem_id:
            self.skipTest("Problem not created")

        num_users = 10
        code = """#include <bits/stdc++.h>
using namespace std;
int main() {
    int a, b, c;
    if (cin >> a >> b >> c) {
        cout << a + b + c << endl;
    }
    return 0;
}"""

        results = []
        lock = Lock()

        def submit_code(user_idx):
            client = APIClient()
            username = f"perf_user_{user_idx}_{int(time.time())}"
            client.post("/api/auth/register", json={"username": username, "password": "123456"})
            client.post("/api/auth/login", json={"username": username, "password": "123456"})

            start = time.time()
            resp = client.post("/api/submissions", json={"problem_id": self.problem_id, "code": code, "language": "cpp"})
            elapsed = time.time() - start

            with lock:
                results.append({
                    "user_idx": user_idx,
                    "status_code": resp.status_code,
                    "submission_id": resp.json().get("id") if resp.status_code == 201 else None,
                    "submit_time": elapsed
                })
            return resp.status_code == 201

        with ThreadPoolExecutor(max_workers=num_users) as executor:
            futures = [executor.submit(submit_code, i) for i in range(num_users)]
            for future in as_completed(futures):
                pass

        for r in results:
            self.assertEqual(r["status_code"], 201, f"User {r['user_idx']} submission failed")
            self.assertLess(r["submit_time"], 5.0, f"User {r['user_idx']} submit time {r['submit_time']:.3f}s > 5s")
            self.assertIsNotNone(r["submission_id"])

    def test_judgment_latency(self):
        if not self.problem_id:
            self.skipTest("Problem not created")

        code = """#include <bits/stdc++.h>
using namespace std;
int main() {
    int a, b, c;
    if (cin >> a >> b >> c) {
        cout << a + b + c << endl;
    }
    return 0;
}"""

        client = APIClient()
        username = f"latency_user_{int(time.time())}"
        client.post("/api/auth/register", json={"username": username, "password": "123456"})
        client.post("/api/auth/login", json={"username": username, "password": "123456"})

        submit_resp = client.post("/api/submissions", json={"problem_id": self.problem_id, "code": code, "language": "cpp"})
        self.assertEqual(submit_resp.status_code, 201)
        submission_id = submit_resp.json()["id"]

        start = time.time()
        for _ in range(30):
            time.sleep(1)
            r = client.get(f"/api/submissions/{submission_id}")
            if r.json().get("queue_status") in ["completed", "failed"]:
                elapsed = time.time() - start
                self.assertEqual(r.json().get("status"), "AC")
                self.assertLess(elapsed, 20.0, f"Judgment latency {elapsed:.3f}s > 20s")
                return
        self.fail("Timeout waiting for judgment")


if __name__ == "__main__":
    unittest.main(verbosity=2)
