async function checkAuth() {
    try {
        const result = await api.auth.me();
        return result && result.username;
    } catch {
        return false;
    }
}

function getCurrentUser() {
    return JSON.parse(sessionStorage.getItem('user') || 'null');
}

function setCurrentUser(user) {
    if (user) {
        sessionStorage.setItem('user', JSON.stringify(user));
    } else {
        sessionStorage.removeItem('user');
    }
}