const API_BASE = '/api';

async function request(method, path, body = null) {
    const options = {
        method,
        headers: {
            'Content-Type': 'application/json'
        }
    };

    if (body) {
        options.body = JSON.stringify(body);
    }

    const response = await fetch(API_BASE + path, options);
    const text = await response.text();

    try {
        return JSON.parse(text);
    } catch {
        return { error: text };
    }
}

async function uploadTestCase(problemId, inputFile, outputFile, isSample = false) {
    const formData = new FormData();
    formData.append('input', inputFile);
    formData.append('output', outputFile);
    if (isSample) {
        formData.append('is_sample', 'true');
    }

    const response = await fetch(API_BASE + '/problems/' + problemId + '/testcases', {
        method: 'POST',
        body: formData
    });

    const text = await response.text();
    try {
        return JSON.parse(text);
    } catch {
        return { error: text };
    }
}

const api = {
    auth: {
        async register(username, password) {
            return request('POST', '/auth/register', { username, password });
        },
        async login(username, password) {
            return request('POST', '/auth/login', { username, password });
        },
        async logout() {
            return request('POST', '/auth/logout');
        },
        async me() {
            return request('GET', '/auth/me');
        },
        async deleteAccount() {
            return request('DELETE', '/auth/me');
        }
    },
    problems: {
        async list(params = {}) {
            const query = new URLSearchParams();
            if (params.page) query.set('page', params.page);
            if (params.pageSize) query.set('pageSize', params.pageSize);
            if (params.difficulty) query.set('difficulty', params.difficulty);
            if (params.search) query.set('search', params.search);
            if (params.tags && params.tags.length > 0) query.set('tags', params.tags.join(','));

            const queryStr = query.toString();
            return request('GET', '/problems' + (queryStr ? '?' + queryStr : ''));
        },
        async get(id) {
            return request('GET', '/problems/' + id);
        },
        async create(data) {
            return request('POST', '/problems', data);
        },
        async update(id, data) {
            return request('PUT', '/problems/' + id, data);
        },
        async delete(id) {
            return request('DELETE', '/problems/' + id);
        },
        async getTestCases(problemId) {
            return request('GET', '/problems/' + problemId + '/testcases');
        },
        async listTags() {
            return request('GET', '/problems/tags');
        }
    },
    testCases: {
        async add(problemId, inputFile, outputFile, isSample = false) {
            return uploadTestCase(problemId, inputFile, outputFile, isSample);
        },
        async delete(id) {
            return request('DELETE', '/testcases/' + id);
        }
    },
    submissions: {
        async create(data) {
            return request('POST', '/submissions', data);
        },
        async list() {
            return request('GET', '/submissions');
        },
        async get(id) {
            return request('GET', '/submissions/' + id);
        }
    },
    stats: {
        async get() {
            return request('GET', '/stats');
        }
    }
};