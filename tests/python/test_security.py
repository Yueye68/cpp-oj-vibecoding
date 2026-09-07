import os
import tempfile
import time

import pytest
import requests


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


@pytest.fixture(scope="module")
def permission_user():
    c = APIClient()
    resp = c.post("/api/auth/register", json={"username": "security_user", "password": "123456"})
    c.post("/api/auth/login", json={"username": "security_user", "password": "123456"})
    return c


@pytest.fixture(scope="module")
def code_exec_setup():
    user_client = APIClient()
    user_client.post("/api/auth/register", json={"username": "code_user", "password": "123456"})
    user_client.post("/api/auth/login", json={"username": "code_user", "password": "123456"})

    admin = APIClient()
    admin.post("/api/auth/login", json={"username": "admin", "password": "admin123"})
    resp = admin.post(
        "/api/problems",
        json={
            "title": "安全测试",
            "difficulty": "easy",
            "tags": ["测试"],
            "time_limit_ms": 5000,
            "memory_limit_mb": 256,
        },
    )
    problem_id = resp.json().get("id")

    inp = tempfile.NamedTemporaryFile(mode="w", delete=False)
    inp.write("1")
    inp.close()
    out = tempfile.NamedTemporaryFile(mode="w", delete=False)
    out.write("1")
    out.close()
    temp_files = [inp.name, out.name]

    with open(inp.name, "rb") as f_in, open(out.name, "rb") as f_out:
        admin.upload_file(
            f"/api/problems/{problem_id}/testcases",
            files={"input": f_in, "output": f_out, "is_sample": "1"},
        )

    yield {"user_client": user_client, "problem_id": problem_id, "temp_files": temp_files}

    for f in temp_files:
        if f and os.path.exists(f):
            os.unlink(f)


def _poll_submission(client, sub_id, timeout=30):
    for _ in range(timeout):
        r = client.get(f"/api/submissions/{sub_id}")
        if r.json().get("queue_status") in ["completed", "failed"]:
            return r.json()
        time.sleep(1)
    return None


class TestPermissionControl:
    def test_user_cannot_create_problem(self, permission_user):
        resp = permission_user.post(
            "/api/problems",
            json={"title": "Test", "difficulty": "easy"},
        )
        assert resp.status_code == 403

    def test_user_cannot_delete_problem(self, permission_user):
        resp = permission_user.delete("/api/problems/1")
        assert resp.status_code == 403

    def test_user_cannot_upload_testcase(self, permission_user):
        resp = permission_user.post("/api/problems/1/testcases")
        assert resp.status_code == 403

    def test_user_cannot_delete_testcase(self, permission_user):
        resp = permission_user.delete("/api/testcases/1")
        assert resp.status_code == 403


class TestTestcaseIsolation:
    def test_cannot_access_testcase_files(self):
        resp = requests.get(f"{BASE_URL}/uploads/test_cases/1/input.txt")
        assert resp.status_code in [403, 404]

    def test_cannot_access_uploads_dir(self):
        resp = requests.get(f"{BASE_URL}/uploads/")
        assert resp.status_code in [403, 404]


class TestCodeExecutionIsolation:
    def test_fork_bomb_isolation(self, code_exec_setup):
        user_client = code_exec_setup["user_client"]
        problem_id = code_exec_setup["problem_id"]
        code = """#include <bits/stdc++.h>
int main() {
    while(fork()) {}
    return 0;
}"""
        resp = user_client.post(
            "/api/submissions",
            json={"problem_id": problem_id, "code": code, "language": "cpp"},
        )
        assert resp.status_code == 201
        sub_id = resp.json()["id"]

        result = _poll_submission(user_client, sub_id, timeout=30)
        assert result is not None
        assert result.get("status") in ["RE", "TLE"]

    def test_infinite_loop_isolation(self, code_exec_setup):
        user_client = code_exec_setup["user_client"]
        problem_id = code_exec_setup["problem_id"]
        code = """#include <bits/stdc++.h>
int main() {
    while(true) {}
    return 0;
}"""
        resp = user_client.post(
            "/api/submissions",
            json={"problem_id": problem_id, "code": code, "language": "cpp"},
        )
        assert resp.status_code == 201
        sub_id = resp.json()["id"]

        result = _poll_submission(user_client, sub_id, timeout=30)
        assert result is not None
        assert result.get("status") == "TLE"

    def test_memory_limit_isolation(self, code_exec_setup):
        user_client = code_exec_setup["user_client"]
        problem_id = code_exec_setup["problem_id"]
        code = """#include <bits/stdc++.h>
int main() {
    std::vector<int> v(1000000000, 1);
    return 0;
}"""
        resp = user_client.post(
            "/api/submissions",
            json={"problem_id": problem_id, "code": code, "language": "cpp"},
        )
        assert resp.status_code == 201
        sub_id = resp.json()["id"]

        result = _poll_submission(user_client, sub_id, timeout=30)
        assert result is not None
        assert result.get("status") in ["MLE", "RE"]


if __name__ == "__main__":
    import sys
    import pytest
    sys.exit(pytest.main([__file__, "-v"]))
