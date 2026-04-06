// ===== CHECK SESSION ON PAGE LOAD =====
document.addEventListener('DOMContentLoaded', function() {
    checkSession();
    setupEventListeners();
    loadFormData();
});

// ===== VERIFY AND LOAD SESSION =====
function checkSession() {
    fetch('./cgi-bin/verify_session.py?t=' + Date.now(), {
        method: 'GET',
        credentials: 'include',
        headers: {
            'Cache-Control': 'no-cache, no-store, must-revalidate',
            'Pragma': 'no-cache',
            'Expires': '0'
        }
    })
    .then(response => response.json())
    .then(data => {
        if (data.authenticated) {
            // Session is valid
            document.getElementById('userInfo').textContent = `👤 ${data.username}`;
            console.log('✅ Sessão válida para:', data.username);
        } else {
            // No valid session, redirect to login
            console.log('❌ Sem sessão válida, redirecionando para login...');
            // Clear any stale session data
            document.cookie = 'session=; path=/; expires=Thu, 01 Jan 1970 00:00:00 UTC;';
            showError('Sessão expirada, por favor faz login novamente');
            setTimeout(() => {
                window.location.href = './login.html';
            }, 1500);
        }
    })
    .catch(error => {
        console.error('Erro ao verificar sessão:', error);
        // Clear any stale session data
        document.cookie = 'session=; path=/; expires=Thu, 01 Jan 1970 00:00:00 UTC;';
        showError('Erro ao verificar sessão');
        setTimeout(() => {
            window.location.href = './login.html';
        }, 1500);
    });
}

// ===== SETUP EVENT LISTENERS =====
function setupEventListeners() {
    // Tab switching
    document.querySelectorAll('.tab-btn').forEach(btn => {
        btn.addEventListener('click', function() {
            switchTab(this.dataset.tab);
        });
    });

    // Response tab switching
    document.querySelectorAll('.response-tab').forEach(btn => {
        btn.addEventListener('click', function() {
            switchResponseTab(this.dataset.response);
        });
    });

    // HTTP Tester
    document.querySelectorAll('.method-btn').forEach(btn => {
        btn.addEventListener('click', function() {
            selectMethod(this.dataset.method);
        });
    });

    document.querySelectorAll('.content-btn').forEach(btn => {
        btn.addEventListener('click', function() {
            selectContentType(this.dataset.type);
        });
    });

    document.getElementById('sendBtn').addEventListener('click', sendHttpRequest);

    // Form Database
    document.getElementById('dataForm').addEventListener('submit', saveFormData);
    document.getElementById('refreshBtn').addEventListener('click', loadFormData);

    // Logout
    document.getElementById('logoutBtn').addEventListener('click', logout);
}

// ===== TAB SWITCHING =====
function switchTab(tabName) {
    // Hide all tabs
    document.querySelectorAll('.tab-content').forEach(tab => {
        tab.classList.remove('active');
    });

    // Remove active from all buttons
    document.querySelectorAll('.tab-btn').forEach(btn => {
        btn.classList.remove('active');
    });

    // Show selected tab
    document.getElementById(tabName).classList.add('active');

    // Activate button
    document.querySelector(`[data-tab="${tabName}"]`).classList.add('active');
}

function switchResponseTab(tabName) {
    // Hide all response outputs
    document.querySelectorAll('.response-output').forEach(output => {
        output.style.display = 'none';
    });

    // Remove active from buttons
    document.querySelectorAll('.response-tab').forEach(btn => {
        btn.classList.remove('active');
    });

    // Show selected output
    document.getElementById(`response${tabName.charAt(0).toUpperCase() + tabName.slice(1)}`).style.display = 'block';

    // Activate button
    document.querySelector(`[data-response="${tabName}"]`).classList.add('active');
}

// ===== HTTP TESTER METHODS =====
let currentMethod = 'GET';
let currentContentType = 'application/x-www-form-urlencoded';

function selectMethod(method) {
    currentMethod = method;

    // Update button styles
    document.querySelectorAll('.method-btn').forEach(btn => {
        btn.classList.remove('active');
    });
    document.querySelector(`[data-method="${method}"]`).classList.add('active');

    // Show/hide body section based on method
    const bodySection = document.getElementById('bodySection');
    const contentTypeSection = document.getElementById('contentTypeSection');

    if (method === 'POST') {
        bodySection.style.display = 'block';
        contentTypeSection.style.display = 'block';
    } else {
        bodySection.style.display = 'none';
        contentTypeSection.style.display = 'none';
    }
}

function selectContentType(type) {
    currentContentType = type;

    // Update button styles
    document.querySelectorAll('.content-btn').forEach(btn => {
        btn.classList.remove('active');
    });
    document.querySelector(`[data-type="${type}"]`).classList.add('active');
}

function sendHttpRequest() {
    const method = currentMethod;
    const url = document.getElementById('testUrl').value.trim();
    const body = document.getElementById('testBody').value.trim();

    if (!url) {
        showError('Por favor, insere uma URL');
        return;
    }

    const btn = document.getElementById('sendBtn');
    btn.disabled = true;
    btn.innerHTML = '⏳ Enviando...';

    const options = {
        method: method,
        credentials: 'include'
    };

    if (method === 'POST' && body) {
        options.headers = {
            'Content-Type': currentContentType
        };
        options.body = body;
    }

    fetch(url, options)
        .then(response => {
            const status = `${response.status} ${response.statusText}`;
            const headers = {};
            response.headers.forEach((value, key) => {
                headers[key] = value;
            });

            document.getElementById('responseStatus').textContent = status;
            document.getElementById('responseHeaders').textContent = JSON.stringify(headers, null, 2);

            return response.text().then(text => {
                // Try to parse as JSON for pretty printing
                try {
                    const json = JSON.parse(text);
                    document.getElementById('responseBody').textContent = JSON.stringify(json, null, 2);
                } catch (e) {
                    document.getElementById('responseBody').textContent = text || '(vazio)';
                }

                // Switch to status tab
                switchResponseTab('status');
                showSuccess('Requisição enviada com sucesso!');
            });
        })
        .catch(error => {
            console.error('Erro:', error);
            document.getElementById('responseStatus').textContent = `ERRO: ${error.message}`;
            showError('Erro ao enviar requisição: ' + error.message);
        })
        .finally(() => {
            btn.disabled = false;
            btn.innerHTML = 'Enviar Requisição 🚀';
        });
}

// ===== FORM DATABASE METHODS =====
function saveFormData(e) {
    e.preventDefault();

    const name = document.getElementById('dataName').value.trim();
    const email = document.getElementById('dataEmail').value.trim();
    const message = document.getElementById('dataMessage').value.trim();

    if (!name || !email) {
        showError('Nome e Email são obrigatórios');
        return;
    }

    const btn = document.querySelector('.btn-save');
    btn.disabled = true;
    btn.innerHTML = '💾 Guardando...';

    fetch('./cgi-bin/form_save.py', {
        method: 'POST',
        credentials: 'include',
        headers: {
            'Content-Type': 'application/x-www-form-urlencoded'
        },
        body: new URLSearchParams({
            name: name,
            email: email,
            message: message
        }).toString()
    })
    .then(response => response.json())
    .then(data => {
        if (data.ok) {
            showSuccess('Dados guardados com sucesso!');
            document.getElementById('dataForm').reset();
            setTimeout(() => {
                loadFormData();
            }, 500);
        } else {
            showError(data.error || 'Erro ao guardar dados');
        }
    })
    .catch(error => {
        console.error('Erro:', error);
        showError('Erro ao guardar dados: ' + error.message);
    })
    .finally(() => {
        btn.disabled = false;
        btn.innerHTML = 'Guardar Dados 💾';
    });
}

function loadFormData() {
    const listContainer = document.getElementById('dataList');
    listContainer.innerHTML = '<p class="loading">Carregando dados...</p>';

    fetch('./cgi-bin/form_list.py', {
        method: 'GET',
        credentials: 'include'
    })
    .then(response => response.json())
    .then(data => {
        if (data.ok && data.entries && data.entries.length > 0) {
            listContainer.innerHTML = '';
            data.entries.forEach(entry => {
                const item = createDataItem(entry);
                listContainer.appendChild(item);
            });
        } else {
            listContainer.innerHTML = '<p class="empty">Nenhum dado guardado ainda</p>';
        }
    })
    .catch(error => {
        console.error('Erro:', error);
        listContainer.innerHTML = '<p class="empty">Erro ao carregar dados</p>';
    });
}

function createDataItem(entry) {
    const item = document.createElement('div');
    item.className = 'data-item';

    const content = document.createElement('div');
    content.className = 'data-item-content';

    const nameEl = document.createElement('div');
    nameEl.className = 'data-item-name';
    nameEl.textContent = `📝 ${entry.name}`;

    const emailEl = document.createElement('div');
    emailEl.className = 'data-item-email';
    emailEl.textContent = entry.email;

    const messageEl = document.createElement('div');
    messageEl.className = 'data-item-message';
    messageEl.textContent = entry.message || '(sem mensagem)';

    const dateEl = document.createElement('div');
    dateEl.className = 'data-item-date';
    dateEl.textContent = entry.created_at || 'data desconhecida';

    content.appendChild(nameEl);
    content.appendChild(emailEl);
    content.appendChild(messageEl);
    content.appendChild(dateEl);

    const deleteBtn = document.createElement('button');
    deleteBtn.className = 'data-item-delete';
    deleteBtn.textContent = '🗑️ Deletar';
    deleteBtn.addEventListener('click', () => deleteFormEntry(entry.id, item));

    item.appendChild(content);
    item.appendChild(deleteBtn);

    return item;
}

function deleteFormEntry(entryId, element) {
    if (!confirm('Tens a certeza que queres deletar este item?')) {
        return;
    }

    fetch('./cgi-bin/form_list.py?id=' + entryId, {
        method: 'DELETE',
        credentials: 'include'
    })
    .then(response => response.json())
    .then(data => {
        if (data.ok) {
            showSuccess('Item deletado com sucesso!');
            element.style.animation = 'slideOutRight 0.3s ease-out';
            setTimeout(() => {
                element.remove();
            }, 300);
        } else {
            showError(data.error || 'Erro ao deletar item');
        }
    })
    .catch(error => {
        console.error('Erro:', error);
        showError('Erro ao deletar item: ' + error.message);
    });
}

// ===== LOGOUT FUNCTION =====
function logout() {
    const btn = document.getElementById('logoutBtn');
    btn.disabled = true;
    btn.innerHTML = '⏳ Saindo...';

    fetch('./cgi-bin/logout.py', {
        method: 'POST',
        credentials: 'include'
    })
    .then(response => response.json())
    .then(data => {
        // Limpar cookie manualmente no navegador
        document.cookie = 'session=; path=/; expires=Thu, 01 Jan 1970 00:00:00 UTC; SameSite=Lax;';
        document.cookie = 'session=; path=/; max-age=0;';
        
        if (data.ok) {
            showSuccess('Logout realizado! Redirecionando...');
            setTimeout(() => {
                window.location.href = './landing.html';
            }, 1500);
        } else {
            showError(data.error || 'Erro ao fazer logout');
            btn.disabled = false;
            btn.innerHTML = 'Logout';
        }
    })
    .catch(error => {
        console.error('Erro:', error);
        // Limpar cookie manualmente mesmo com erro
        document.cookie = 'session=; path=/; expires=Thu, 01 Jan 1970 00:00:00 UTC; SameSite=Lax;';
        document.cookie = 'session=; path=/; max-age=0;';
        
        showError('Erro ao fazer logout, limpando sessão...');
        setTimeout(() => {
            window.location.href = './landing.html';
        }, 1500);
    });
}

// ===== NOTIFICATION FUNCTIONS =====
function createNotification(message, type) {
    const notification = document.createElement('div');
    notification.className = `notification notification-${type}`;
    notification.innerHTML = `
        <span class="notification-icon">${type === 'error' ? '❌' : '✅'}</span>
        <span class="notification-message">${message}</span>
    `;

    if (!document.querySelector('style[data-notification-dash]')) {
        const style = document.createElement('style');
        style.setAttribute('data-notification-dash', 'true');
        style.innerHTML = `
            .notification {
                position: fixed;
                top: 80px;
                right: 20px;
                background: rgba(21, 26, 43, 0.95);
                border-radius: 8px;
                padding: 14px 18px;
                display: flex;
                align-items: center;
                gap: 10px;
                font-size: 13px;
                font-weight: 500;
                z-index: 10000;
                backdrop-filter: blur(10px);
                animation: slideInRight 0.3s ease-out;
            }

            .notification-error {
                border: 1px solid rgba(255, 0, 110, 0.3);
                color: #ff006e;
            }

            .notification-success {
                border: 1px solid rgba(0, 255, 156, 0.3);
                color: #00FF9C;
            }

            .notification.fade-out {
                animation: slideOutRight 0.3s ease-out forwards;
            }

            @keyframes slideInRight {
                from {
                    transform: translateX(400px);
                    opacity: 0;
                }
                to {
                    transform: translateX(0);
                    opacity: 1;
                }
            }

            @keyframes slideOutRight {
                from {
                    transform: translateX(0);
                    opacity: 1;
                }
                to {
                    transform: translateX(400px);
                    opacity: 0;
                }
            }
        `;
        document.head.appendChild(style);
    }

    document.body.appendChild(notification);

    setTimeout(() => {
        notification.classList.add('fade-out');
        setTimeout(() => notification.remove(), 300);
    }, 4000);

    return notification;
}

function showError(message) {
    createNotification(message, 'error');
}

function showSuccess(message) {
    createNotification(message, 'success');
}
