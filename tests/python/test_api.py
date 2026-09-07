import os
import tempfile
import time
from typing import Tuple

import pytest
import requests


BASE_URL = "http://localhost:8080"
DEFAULT_POLL_INTERVAL = 1
DEFAULT_POLL_TIMEOUT = 30


def _unique_name(prefix: str) -> str:
    return f"{prefix}_{int(time.time() * 1000)}"


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


@pytest.fixture
def client():
    c = APIClient()
    yield c
    c.session.close()


@pytest.fixture(scope="module")
def admin_client():
    c = APIClient()
    resp = c.post("/api/auth/login", json={"username": "admin", "password": "admin123"})
    if resp.status_code == 200:
        data = resp.json()
        user_data = data.get("user", data)
        if user_data.get("role") != "admin":
            c = APIClient()
            admin_username = _unique_name("admin")
            c.post("/api/auth/register", json={"username": admin_username, "password": "admin123"})
            c.post("/api/auth/login", json={"username": admin_username, "password": "admin123"})
    yield c
    c.session.close()


@pytest.fixture(scope="module")
def regular_client():
    c = APIClient()
    username = _unique_name("regular")
    c.post("/api/auth/register", json={"username": username, "password": "123456"})
    c.post("/api/auth/login", json={"username": username, "password": "123456"})
    yield c
    c.session.close()


@pytest.fixture(scope="module")
def problem_with_testcase(admin_client):
    resp = admin_client.post(
        "/api/problems",
        json={
            "title": "两数之和",
            "description": "给定一个整数数组nums，返回满足条件的两个数的下标",
            "difficulty": "easy",
            "tags": ["数组", "哈希表"],
            "time_limit_ms": 1000,
            "memory_limit_mb": 256,
        },
    )
    problem_id = resp.json().get("id") if resp.status_code == 201 else None

    input_file = tempfile.NamedTemporaryFile(mode="w", delete=False)
    input_file.write("2\n3")
    input_file.close()
    output_file = tempfile.NamedTemporaryFile(mode="w", delete=False)
    output_file.write("5")
    output_file.close()

    try:
        with open(input_file.name, "rb") as inp, open(output_file.name, "rb") as out:
            admin_client.upload_file(
                f"/api/problems/{problem_id}/testcases",
                files={"input": inp, "output": out, "is_sample": "1"},
            )
        yield problem_id
    finally:
        for f in (input_file.name, output_file.name):
            if os.path.exists(f):
                os.unlink(f)


class TestHealth:
    def test_health_check(self):
        resp = requests.get(f"{BASE_URL}/health")
        assert resp.status_code == 200
        assert resp.text == "OK"


class TestAuth:
    def test_register(self, client):
        username = _unique_name("user")
        resp = client.post(
            "/api/auth/register",
            json={"username": username, "password": "123456"},
        )
        assert resp.status_code in [201, 409]

    def test_login_success(self, client):
        username = _unique_name("logintest")
        client.post("/api/auth/register", json={"username": username, "password": "123456"})
        resp = client.post(
            "/api/auth/login",
            json={"username": username, "password": "123456"},
        )
        assert resp.status_code == 200

    def test_login_wrong_password(self, client):
        username = _unique_name("wrongpwd")
        client.post("/api/auth/register", json={"username": username, "password": "123456"})
        resp = client.post(
            "/api/auth/login",
            json={"username": username, "password": "wrongpass"},
        )
        assert resp.status_code == 401

    def test_get_current_user(self, client):
        username = _unique_name("meuser")
        client.post("/api/auth/register", json={"username": username, "password": "123456"})
        client.post("/api/auth/login", json={"username": username, "password": "123456"})
        resp = client.get("/api/auth/me")
        assert resp.status_code == 200

    def test_logout(self, client):
        username = _unique_name("logoutuser")
        client.post("/api/auth/register", json={"username": username, "password": "123456"})
        client.post("/api/auth/login", json={"username": username, "password": "123456"})
        resp = client.post("/api/auth/logout")
        assert resp.status_code == 200

    def test_delete_account_unauthorized(self, client):
        resp = client.delete("/api/auth/me")
        assert resp.status_code == 401

    def test_delete_account_success(self, client):
        username = _unique_name("deluser")
        client.post("/api/auth/register", json={"username": username, "password": "123456"})
        login_resp = client.post(
            "/api/auth/login",
            json={"username": username, "password": "123456"},
        )
        assert login_resp.status_code == 200
        del_resp = client.delete("/api/auth/me")
        assert del_resp.status_code == 200
        me_resp = client.get("/api/auth/me")
        assert me_resp.status_code == 401
        relogin = client.post(
            "/api/auth/login",
            json={"username": username, "password": "123456"},
        )
        assert relogin.status_code == 401

    def test_admin_cannot_delete_self(self, client):
        login_resp = client.post(
            "/api/auth/login",
            json={"username": "admin", "password": "admin123"},
        )
        if login_resp.status_code == 200:
            data = login_resp.json()
            user_data = data.get("user", data)
            if user_data.get("role") == "admin":
                resp = client.delete("/api/auth/me")
                assert resp.status_code == 403


class TestProblems:
    def test_get_problem_list(self, client):
        resp = client.get("/api/problems", params={"page": 1, "pageSize": 10})
        assert resp.status_code == 200

    def test_get_problem_detail(self, admin_client, problem_with_testcase):
        if not problem_with_testcase:
            pytest.skip("Problem not created")
        resp = admin_client.get(f"/api/problems/{problem_with_testcase}")
        assert resp.status_code == 200

    def test_update_problem(self, admin_client, problem_with_testcase):
        if not problem_with_testcase:
            pytest.skip("Problem not created")
        resp = admin_client.put(
            f"/api/problems/{problem_with_testcase}",
            json={"title": "两数之和 Updated", "difficulty": "medium"},
        )
        assert resp.status_code == 200

    def test_delete_problem(self, admin_client):
        create_resp = admin_client.post(
            "/api/problems",
            json={
                "title": "待删除题目",
                "description": "用于删除测试",
                "difficulty": "easy",
                "tags": ["测试"],
                "time_limit_ms": 1000,
                "memory_limit_mb": 256,
            },
        )
        if create_resp.status_code != 201:
            pytest.skip("Could not create problem for deletion test")
        new_problem_id = create_resp.json().get("id")
        resp = admin_client.delete(f"/api/problems/{new_problem_id}")
        assert resp.status_code == 200

    def test_get_nonexistent_problem(self, client):
        resp = client.get("/api/problems/99999")
        assert resp.status_code == 404

    def test_list_problem_tags(self, client):
        resp = client.get("/api/problems/tags")
        assert resp.status_code == 200
        data = resp.json()
        assert "tags" in data
        assert isinstance(data["tags"], list)
        for item in data["tags"]:
            assert "name" in item and "count" in item

    def test_filter_problems_by_tag(self, client):
        tags_resp = client.get("/api/problems/tags")
        if tags_resp.status_code != 200:
            pytest.skip("tags endpoint unavailable")
        tags = tags_resp.json().get("tags", [])
        if not tags:
            pytest.skip("no tags in db")
        target = tags[0]["name"]
        resp = client.get("/api/problems", params={"tags": target, "pageSize": 50})
        assert resp.status_code == 200
        for p in resp.json().get("problems", []):
            assert target in (p.get("tags") or [])

    def test_delete_nonexistent_problem(self, admin_client, problem_with_testcase):
        if not problem_with_testcase:
            pytest.skip("Problem not created")
        resp = admin_client.delete("/api/problems/99999")
        assert resp.status_code == 404


class TestNonAdminPermissions:
    def test_non_admin_create_problem(self, regular_client):
        resp = regular_client.post(
            "/api/problems",
            json={"title": "Test", "difficulty": "easy"},
        )
        assert resp.status_code == 403


class TestSubmissions:
    def test_create_submission(self, regular_client, problem_with_testcase):
        if not problem_with_testcase:
            pytest.skip("Problem not created")
        resp = regular_client.post(
            "/api/submissions",
            json={
                "problem_id": problem_with_testcase,
                "code": "#include <bits/stdc++.h>\nusing namespace std;\nint main() { return 0; }",
                "language": "cpp",
            },
        )
        assert resp.status_code == 201
        data = resp.json()
        assert data.get("status") == "pending"
        assert data.get("queue_status") == "pending"
        assert "id" in data

    def test_create_submission_with_polling(self, regular_client, problem_with_testcase):
        if not problem_with_testcase:
            pytest.skip("Problem not created")
        resp = regular_client.post(
            "/api/submissions",
            json={
                "problem_id": problem_with_testcase,
                "code": "#include <bits/stdc++.h>\nusing namespace std;\nint main() { return 0; }",
                "language": "cpp",
            },
        )
        assert resp.status_code == 201
        submission_id = resp.json().get("id")
        assert submission_id is not None

        result_data, queue_status = regular_client.poll_submission(submission_id)
        assert queue_status in ["completed", "failed"]
        if queue_status == "completed":
            assert result_data.get("status") in ["AC", "WA", "CE", "TLE", "MLE", "RE", "PE"]

    def test_get_submission_history(self, regular_client):
        resp = regular_client.get("/api/submissions")
        assert resp.status_code == 200
        data = resp.json()
        submissions = data.get("submissions", [])
        for sub in submissions:
            assert "queue_status" in sub

    def test_get_submission_detail(self, regular_client):
        resp = regular_client.get("/api/submissions/1")
        assert resp.status_code in [200, 404]


class TestEndToEnd:
    @pytest.fixture(scope="class")
    def e2e_problem(self, admin_client):
        resp = admin_client.post(
            "/api/problems",
            json={
                "title": "两数之和-完整测试",
                "description": "给定一个整数数组 nums 和一个目标值 target",
                "difficulty": "easy",
                "tags": ["数组", "哈希表"],
                "time_limit_ms": 1000,
                "memory_limit_mb": 256,
            },
        )
        problem_id = resp.json().get("id") if resp.status_code == 201 else None
        yield problem_id

    def test_problem_list_pagination(self, regular_client):
        resp = regular_client.get("/api/problems", params={"page": 1, "pageSize": 5})
        assert resp.status_code == 200
        data = resp.json()
        assert "problems" in data
        assert "total" in data
        assert len(data.get("problems", [])) <= 5

    def test_problem_filter_difficulty(self, regular_client):
        resp = regular_client.get("/api/problems", params={"difficulty": "easy"})
        assert resp.status_code == 200
        data = resp.json()
        for p in data.get("problems", []):
            assert p.get("difficulty") == "easy"

    def test_submission_detail_results(self, regular_client, e2e_problem):
        if not e2e_problem:
            pytest.skip("Problem not created")
        resp = regular_client.post(
            "/api/submissions",
            json={
                "problem_id": e2e_problem,
                "code": "#include <bits/stdc++.h>\nint main() { return 0; }",
                "language": "cpp",
            },
        )
        assert resp.status_code == 201
        submission_id = resp.json().get("id")

        result_data, queue_status = regular_client.poll_submission(submission_id, timeout=60)
        assert queue_status == "completed"

        detail_resp = regular_client.get(f"/api/submissions/{submission_id}")
        assert detail_resp.status_code == 200
        assert "results" in detail_resp.json()


if __name__ == "__main__":
    import sys
    import pytest
    sys.exit(pytest.main([__file__, "-v"]))
