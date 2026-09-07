const FALLBACK_TAGS = ['算法', '数据结构', '字符串', '数组', '动态规划', '图论', '数学', '搜索', '排序', '贪心'];

let currentPage = 1;
let currentFilters = {
    difficulty: '',
    tags: [],
    search: ''
};
let availableTags = [];

async function loadProblems() {
    const container = document.getElementById('problems-container');
    const pagination = document.getElementById('pagination');

    container.innerHTML = '<div class="loading">加载中...</div>';

    try {
        const result = await api.problems.list({
            page: currentPage,
            pageSize: 20,
            difficulty: currentFilters.difficulty,
            search: currentFilters.search,
            tags: currentFilters.tags
        });

        if (result.error) {
            container.innerHTML = '<div class="error">加载失败: ' + result.error + '</div>';
            return;
        }

        renderProblems(container, result.problems || []);
        renderPagination(pagination, result.page, result.totalPages, result.total);
    } catch (e) {
        container.innerHTML = '<div class="error">加载失败: ' + e.message + '</div>';
    }
}

function renderProblems(container, problems) {
    if (problems.length === 0) {
        const hasFilter = currentFilters.tags.length > 0 || currentFilters.difficulty || currentFilters.search;
        const msg = hasFilter
            ? '当前筛选条件下暂无题目，<a href="javascript:location.reload()">清除筛选</a>'
            : '暂无题目';
        container.innerHTML = `<div class="empty">${msg}</div>`;
        return;
    }

    container.innerHTML = problems.map(p => {
        const difficultyClass = 'difficulty-' + p.difficulty;
        const difficultyText = { easy: '简单', medium: '中等', hard: '困难' }[p.difficulty] || p.difficulty;
        const tagsHtml = (p.tags || []).map(tag => `<span class="tag">${tag}</span>`).join('');

        return `
            <div class="problem-card" onclick="window.location.href='/problem.html?id=${p.id}'">
                <div class="problem-header">
                    <span class="problem-id">#${p.id}</span>
                    <span class="problem-title">${escapeHtml(p.title)}</span>
                </div>
                <div class="problem-meta">
                    <span class="difficulty ${difficultyClass}">${difficultyText}</span>
                    ${tagsHtml}
                </div>
            </div>
        `;
    }).join('');
}

function renderPagination(container, page, totalPages, total) {
    if (totalPages <= 1) {
        container.innerHTML = `<div class="pagination-info">共 ${total} 题</div>`;
        return;
    }

    let html = `<div class="pagination-info">共 ${total} 题，第 ${page}/${totalPages} 页</div>`;
    html += '<div class="pagination-buttons">';

    if (page > 1) {
        html += `<button onclick="goToPage(${page - 1})">上一页</button>`;
    }

    const start = Math.max(1, page - 2);
    const end = Math.min(totalPages, page + 2);

    if (start > 1) {
        html += `<button onclick="goToPage(1)">1</button>`;
        if (start > 2) {
            html += '<span class="pagination-ellipsis">...</span>';
        }
    }

    for (let i = start; i <= end; i++) {
        html += `<button onclick="goToPage(${i})" class="${i === page ? 'active' : ''}">${i}</button>`;
    }

    if (end < totalPages) {
        if (end < totalPages - 1) {
            html += '<span class="pagination-ellipsis">...</span>';
        }
        html += `<button onclick="goToPage(${totalPages})">${totalPages}</button>`;
    }

    if (page < totalPages) {
        html += `<button onclick="goToPage(${page + 1})">下一页</button>`;
    }

    html += '</div>';
    container.innerHTML = html;
}

function goToPage(page) {
    currentPage = page;
    loadProblems();
    window.scrollTo(0, 0);
}

function setDifficultyFilter(difficulty) {
    currentFilters.difficulty = difficulty;
    currentPage = 1;
    updateFilterUI();
    loadProblems();
}

function toggleTagFilter(tag) {
    const index = currentFilters.tags.indexOf(tag);
    if (index > -1) {
        currentFilters.tags.splice(index, 1);
    } else {
        currentFilters.tags.push(tag);
    }
    currentPage = 1;
    updateFilterUI();
    loadProblems();
}

function updateFilterUI() {
    document.querySelectorAll('.difficulty-btn').forEach(btn => {
        btn.classList.toggle('active', btn.dataset.difficulty === currentFilters.difficulty);
    });

    document.querySelectorAll('.tag-filter').forEach(btn => {
        btn.classList.toggle('active', currentFilters.tags.includes(btn.dataset.tag));
    });
}

function handleSearch() {
    const searchInput = document.getElementById('search-input');
    currentFilters.search = searchInput.value.trim();
    currentPage = 1;
    loadProblems();
}

function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

function initProblemsPage() {
    loadTagFilters();
    loadProblems();

    const searchInput = document.getElementById('search-input');
    if (searchInput) {
        searchInput.addEventListener('keyup', (e) => {
            if (e.key === 'Enter') {
                handleSearch();
            }
        });
    }
}

async function loadTagFilters() {
    const container = document.getElementById('tags-filter');
    if (!container) return;

    let tags = FALLBACK_TAGS;
    try {
        const result = await api.problems.listTags();
        if (result && Array.isArray(result.tags) && result.tags.length > 0) {
            availableTags = result.tags;
            tags = result.tags.map(t => t.name);
        } else {
            availableTags = tags.map(name => ({ name, count: 0 }));
        }
    } catch (e) {
        availableTags = tags.map(name => ({ name, count: 0 }));
    }

    container.innerHTML = tags.map(tag => `
        <button class="tag-filter" data-tag="${tag}" onclick="toggleTagFilter('${tag}')">${tag}</button>
    `).join('');
    updateFilterUI();
}