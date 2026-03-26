/* ──────────────────────────────────────────────
   TAB SWITCHING
   ────────────────────────────────────────────── */
document.querySelectorAll('.tab-btn').forEach(btn => {
    btn.addEventListener('click', () => {
        document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
        document.querySelectorAll('.tab-panel').forEach(p => p.classList.remove('active'));
        btn.classList.add('active');
        document.getElementById(btn.dataset.tab).classList.add('active');
    });
});

/* ──────────────────────────────────────────────
   RESPONSE TAB SWITCHING
   ────────────────────────────────────────────── */
function switchRespTab(tabEl, contentId) {
    const parent = tabEl.closest('.response-area');
    parent.querySelectorAll('.resp-tab').forEach(t => t.classList.remove('active'));
    parent.querySelectorAll('.resp-content').forEach(c => c.classList.remove('active'));
    tabEl.classList.add('active');
    document.getElementById(contentId).classList.add('active');
}

/* ──────────────────────────────────────────────
   HELPER: Display response
   ────────────────────────────────────────────── */
function displayResponse(prefix, status, statusText, headers, body, elapsed) {
    const area = document.getElementById(prefix + '-response');
    area.classList.add('visible');

    // Status
    const code = status;
    const dotEl = document.getElementById(prefix + '-dot');
    const statusEl = document.getElementById(prefix + '-status');
    const timeEl = document.getElementById(prefix + '-time');

    statusEl.textContent = code + ' ' + statusText;
    timeEl.textContent = elapsed + 'ms';

    // Dot + color class
    dotEl.className = 'status-dot';
    statusEl.className = 'status-code';

    if (code >= 200 && code < 300) {
        dotEl.classList.add('success');
        statusEl.classList.add('s2xx');
    } else if (code >= 300 && code < 400) {
        dotEl.classList.add('redirect');
        statusEl.classList.add('s3xx');
    } else if (code >= 400 && code < 500) {
        dotEl.classList.add('error');
        statusEl.classList.add('s4xx');
    } else {
        dotEl.classList.add('error');
        statusEl.classList.add('s5xx');
    }

    // Headers
    let headerStr = '';
    headers.forEach((value, key) => {
        headerStr += key + ': ' + value + '\n';
    });
    const headersEl = document.getElementById(prefix + '-headers');
    if (headersEl) headersEl.textContent = headerStr || '(no headers)';

    // Body
    const bodyEl = document.getElementById(prefix + '-body') || document.getElementById(prefix + '-body-resp');
    if (bodyEl) bodyEl.textContent = body || '(empty response)';
}

/* ──────────────────────────────────────────────
   GET
   ────────────────────────────────────────────── */
async function sendGet() {
    const url = document.getElementById('get-url').value || '/';
    const start = performance.now();
    try {
        const resp = await fetch(url, { method: 'GET', redirect: 'manual' });
        const elapsed = Math.round(performance.now() - start);
        const body = await resp.text();
        displayResponse('get', resp.status, resp.statusText, resp.headers, body, elapsed);
    } catch (err) {
        const elapsed = Math.round(performance.now() - start);
        displayResponse('get', 0, 'Network Error', new Headers(), err.message, elapsed);
    }
}

/* ──────────────────────────────────────────────
   POST
   ────────────────────────────────────────────── */
async function sendPost() {
    const url = document.getElementById('post-url').value || '/uploads';
    const contentType = document.getElementById('post-content-type').value;
    const bodyData = document.getElementById('post-body').value;
    const start = performance.now();
    try {
        const resp = await fetch(url, {
            method: 'POST',
            headers: { 'Content-Type': contentType },
            body: bodyData
        });
        const elapsed = Math.round(performance.now() - start);
        const body = await resp.text();
        displayResponse('post', resp.status, resp.statusText, resp.headers, body, elapsed);
    } catch (err) {
        const elapsed = Math.round(performance.now() - start);
        displayResponse('post', 0, 'Network Error', new Headers(), err.message, elapsed);
    }
}

/* ──────────────────────────────────────────────
   UPLOAD
   ────────────────────────────────────────────── */
function updateFileName() {
    const input = document.getElementById('upload-file');
    const label = document.getElementById('upload-file-name');
    label.textContent = input.files.length > 0 ? input.files[0].name : 'Nenhum ficheiro selecionado';
}

async function sendUpload() {
    const url = document.getElementById('upload-url').value || '/uploads';
    const fileInput = document.getElementById('upload-file');

    if (!fileInput.files.length) {
        alert('Selecione um ficheiro primeiro!');
        return;
    }

    const formData = new FormData();
    formData.append('file', fileInput.files[0]);

    const start = performance.now();
    try {
        const resp = await fetch(url, {
            method: 'POST',
            body: formData
        });
        const elapsed = Math.round(performance.now() - start);
        const body = await resp.text();
        displayResponse('upload', resp.status, resp.statusText, resp.headers, body, elapsed);
    } catch (err) {
        const elapsed = Math.round(performance.now() - start);
        displayResponse('upload', 0, 'Network Error', new Headers(), err.message, elapsed);
    }
}

/* ──────────────────────────────────────────────
   DELETE
   ────────────────────────────────────────────── */
async function sendDelete() {
    const url = document.getElementById('delete-url').value || '/uploads/test.txt';
    const start = performance.now();
    try {
        const resp = await fetch(url, { method: 'DELETE' });
        const elapsed = Math.round(performance.now() - start);
        const body = await resp.text();
        displayResponse('delete', resp.status, resp.statusText, resp.headers, body, elapsed);
    } catch (err) {
        const elapsed = Math.round(performance.now() - start);
        displayResponse('delete', 0, 'Network Error', new Headers(), err.message, elapsed);
    }
}

/* ──────────────────────────────────────────────
   ERROR TRIGGERS
   ────────────────────────────────────────────── */
async function triggerError(code) {
    let url = '/';
    switch (code) {
        case 403:
            // Request a path that should be forbidden
            url = '/errors/';
            break;
        case 404:
            url = '/this-page-does-not-exist-ninja-404';
            break;
        case 405:
            // Send a method not allowed (DELETE on root)
            return triggerMethodNotAllowed();
        case 500:
            url = '/trigger-internal-error';
            break;
    }

    const start = performance.now();
    try {
        const resp = await fetch(url, { method: 'GET' });
        const elapsed = Math.round(performance.now() - start);
        const body = await resp.text();
        displayResponse('error', resp.status, resp.statusText, resp.headers, body, elapsed);
    } catch (err) {
        const elapsed = Math.round(performance.now() - start);
        displayResponse('error', 0, 'Network Error', new Headers(), err.message, elapsed);
    }
}

async function triggerMethodNotAllowed() {
    const start = performance.now();
    try {
        const resp = await fetch('/', { method: 'DELETE' });
        const elapsed = Math.round(performance.now() - start);
        const body = await resp.text();
        displayResponse('error', resp.status, resp.statusText, resp.headers, body, elapsed);
    } catch (err) {
        const elapsed = Math.round(performance.now() - start);
        displayResponse('error', 0, 'Network Error', new Headers(), err.message, elapsed);
    }
}

/* ──────────────────────────────────────────────
   RAW HTTP TESTER
   ────────────────────────────────────────────── */
async function sendRaw() {
    const rawText = document.getElementById('raw-input').value.trim();
    if (!rawText) return;

    // Parse the raw HTTP request
    const lines = rawText.split('\n');
    const requestLine = lines[0].trim().split(' ');
    const method = requestLine[0] || 'GET';
    const path = requestLine[1] || '/';

    // Parse headers
    const headers = {};
    let bodyStart = -1;
    for (let i = 1; i < lines.length; i++) {
        const line = lines[i].trim();
        if (line === '') {
            bodyStart = i + 1;
            break;
        }
        const colonIdx = line.indexOf(':');
        if (colonIdx !== -1) {
            const key = line.substring(0, colonIdx).trim();
            const value = line.substring(colonIdx + 1).trim();
            headers[key] = value;
        }
    }

    let body = undefined;
    if (bodyStart > 0 && bodyStart < lines.length) {
        body = lines.slice(bodyStart).join('\n');
    }

    const fetchOptions = {
        method: method,
        headers: headers
    };
    if (body && method !== 'GET' && method !== 'HEAD') {
        fetchOptions.body = body;
    }

    const start = performance.now();
    try {
        const resp = await fetch(path, fetchOptions);
        const elapsed = Math.round(performance.now() - start);
        const respBody = await resp.text();
        displayResponse('raw', resp.status, resp.statusText, resp.headers, respBody, elapsed);
    } catch (err) {
        const elapsed = Math.round(performance.now() - start);
        displayResponse('raw', 0, 'Network Error', new Headers(), err.message, elapsed);
    }
}

/* ──────────────────────────────────────────────
   FORMULÁRIO + DATABASE (Python save + PHP list)
   ────────────────────────────────────────────── */
async function saveToDB() {
    const name = document.getElementById('form-name').value.trim();
    const email = document.getElementById('form-email').value.trim();
    const message = document.getElementById('form-message').value.trim();

    if (!name || !email) {
        alert('Nome e Email são obrigatórios');
        return;
    }

    const start = performance.now();
    try {
        const params = new URLSearchParams({
            name: name,
            email: email,
            message: message
        });

        const resp = await fetch('/cgi-bin/form_save.py', {
            method: 'POST',
            headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
            body: params
        });

        const elapsed = Math.round(performance.now() - start);
        const data = await resp.json();

        // Show response
        document.getElementById('form-response').style.display = 'block';
        document.getElementById('form-status').textContent = resp.status + ' ' + resp.statusText;
        document.getElementById('form-time').textContent = elapsed + 'ms';
        document.getElementById('form-json').textContent = JSON.stringify(data, null, 2);

        const dot = document.getElementById('form-dot');
        dot.className = 'status-dot ' + (resp.ok ? 'success' : 'error');

        if (data.ok) {
            // Limpar form
            document.getElementById('form-name').value = '';
            document.getElementById('form-email').value = '';
            document.getElementById('form-message').value = '';
            // Atualizar lista
            setTimeout(() => loadFromDB(), 500);
        }
    } catch (err) {
        const elapsed = Math.round(performance.now() - start);
        document.getElementById('form-response').style.display = 'block';
        document.getElementById('form-status').textContent = 'Error';
        document.getElementById('form-time').textContent = elapsed + 'ms';
        document.getElementById('form-json').textContent = err.message;
        document.getElementById('form-dot').className = 'status-dot error';
    }
}

async function loadFromDB() {
    const start = performance.now();
    try {
        const resp = await fetch('/cgi-bin/form_list.php', {
            method: 'GET'
        });
        const elapsed = Math.round(performance.now() - start);
        const data = await resp.json();

        // Show response
        document.getElementById('form-response').style.display = 'block';
        document.getElementById('form-status').textContent = resp.status + ' ' + resp.statusText;
        document.getElementById('form-time').textContent = elapsed + 'ms';
        document.getElementById('form-json').textContent = JSON.stringify(data, null, 2);

        const dot = document.getElementById('form-dot');
        dot.className = 'status-dot ' + (resp.ok ? 'success' : 'error');

        // Display entries
        const listEl = document.getElementById('form-list');
        if (data.ok && data.entries && data.entries.length > 0) {
            listEl.innerHTML = data.entries.map(e => 
                '<div style="margin-bottom:10px;padding:8px;background:rgba(255,255,255,0.05);border-radius:4px;">' +
                '<div style="color:var(--neon);" >' + htmlEscape(e.name) + ' &lt;' + htmlEscape(e.email) + '&gt;</div>' +
                (e.message ? '<div style="color:var(--text-muted);font-size:11px;margin-top:4px;">' + htmlEscape(e.message) + '</div>' : '') +
                '<div style="display:flex;justify-content:space-between;align-items:center;margin-top:6px;">' +
                    '<div style="color:var(--dark-gray);font-size:10px;">' + htmlEscape(e.created_at || '') + '</div>' +
                    '<button class="btn btn-red" style="padding:4px 8px;font-size:10px;min-height:auto;" onclick="deleteFromDB(' + Number(e.id || 0) + ')">Apagar</button>' +
                '</div>' +
                '</div>'
            ).join('');
        } else {
            listEl.innerHTML = '<span style="color:var(--dark-gray);">Sem dados.</span>';
        }
    } catch (err) {
        const elapsed = Math.round(performance.now() - start);
        document.getElementById('form-response').style.display = 'block';
        document.getElementById('form-status').textContent = 'Error';
        document.getElementById('form-time').textContent = elapsed + 'ms';
        document.getElementById('form-json').textContent = err.message;
        document.getElementById('form-dot').className = 'status-dot error';
    }
}

function htmlEscape(str) {
    return String(str)
        .replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;');
}

async function deleteFromDB(id) {
    const safeId = Number(id);
    if (!safeId) return;
    if (!confirm('Remover este registro?')) return;

    const start = performance.now();
    try {
        const resp = await fetch('/cgi-bin/form_list.php?id=' + encodeURIComponent(String(safeId)), {
            method: 'DELETE'
        });
        const elapsed = Math.round(performance.now() - start);
        const data = await resp.json();

        document.getElementById('form-response').style.display = 'block';
        document.getElementById('form-status').textContent = resp.status + ' ' + resp.statusText;
        document.getElementById('form-time').textContent = elapsed + 'ms';
        document.getElementById('form-json').textContent = JSON.stringify(data, null, 2);
        document.getElementById('form-dot').className = 'status-dot ' + (resp.ok && data.ok ? 'success' : 'error');

        if (resp.ok && data.ok) {
            await loadFromDB();
        }
    } catch (err) {
        const elapsed = Math.round(performance.now() - start);
        document.getElementById('form-response').style.display = 'block';
        document.getElementById('form-status').textContent = 'Error';
        document.getElementById('form-time').textContent = elapsed + 'ms';
        document.getElementById('form-json').textContent = err.message;
        document.getElementById('form-dot').className = 'status-dot error';
    }
}

/* ──────────────────────────────────────────────
   FADE-IN ON SCROLL
   ────────────────────────────────────────────── */
const observer = new IntersectionObserver((entries) => {
    entries.forEach(entry => {
        if (entry.isIntersecting) {
            entry.target.classList.add('visible');
        }
    });
}, { threshold: 0.1 });

document.querySelectorAll('.fade-in').forEach(el => observer.observe(el));

/* ──────────────────────────────────────────────
   KEYBOARD SHORTCUT: Enter to send on GET input
   ────────────────────────────────────────────── */
document.getElementById('get-url').addEventListener('keydown', e => {
    if (e.key === 'Enter') sendGet();
});
document.getElementById('delete-url').addEventListener('keydown', e => {
    if (e.key === 'Enter') sendDelete();
});