// ===== CHECK SESSION ON PAGE LOAD =====
document.addEventListener('DOMContentLoaded', function() {
    checkExistingSession();
});

function checkExistingSession() {
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
            console.log('✅ Sessão válida detectada, redirecionando para dashboard...');
            window.location.href = './dashboard.html';
        } else {
            console.log('❌ Sem sessão válida, permitindo login');
            // Clear any stale session data
            document.cookie = 'session=; path=/; expires=Thu, 01 Jan 1970 00:00:00 UTC;';
        }
    })
    .catch(error => {
        console.log('Sem sessão válida, permitindo login:', error);
        // Clear any stale session data
        document.cookie = 'session=; path=/; expires=Thu, 01 Jan 1970 00:00:00 UTC;';
    });
}

// ===== FORM SUBMISSION =====
document.getElementById('loginForm').addEventListener('submit', function(e) {
    e.preventDefault();

    const username = document.getElementById('username').value.trim();
    const password = document.getElementById('password').value.trim();

    // Basic validation
    if (!username || !password) {
        showError('Por favor, preenche todos os campos');
        return;
    }

    if (password.length < 6) {
        showError('A senha deve ter pelo menos 6 caracteres');
        return;
    }

    // Submit to server
    submitLogin(username, password);
});

// ===== FORM SUBMISSION HANDLER =====
function submitLogin(username, password) {
    const btn = document.querySelector('.btn-login');
    btn.disabled = true;
    btn.innerHTML = '<span class="spinner"></span> Autenticando...';

    // Send POST request to server
    fetch('./cgi-bin/login.py', {
        method: 'POST',
        credentials: 'include', // Include cookies in request
        headers: {
            'Content-Type': 'application/x-www-form-urlencoded'
        },
        body: new URLSearchParams({
            username: username,
            password: password
        }).toString()
    })
    .then(response => {
        // Check if response is OK (status 200-299)
        if (!response.ok) {
            // Try to parse error message from JSON
            return response.json().then(data => {
                throw new Error(data.message || `Erro ${response.status}: ${response.statusText}`);
            }).catch(parseError => {
                // If JSON parse fails, use generic error
                throw new Error(`Erro ${response.status}: ${response.statusText}`);
            });
        }
        return response.json();
    })
    .then(data => {
        if (data.ok) {
            console.log('✅ Login bem-sucedido! Dados recebidos:', data);
            console.log('📍 Verificando cookies após login...');
            console.log('🍪 Cookies disponíveis:', document.cookie);
            showSuccess('Login realizado! Redirecionando...');

            // Redirect after delay
            setTimeout(() => {
                console.log('🍪 Cookies antes do redirect:', document.cookie);
                console.log('🔄 Redirecionando para /dashboard.html...');
                window.location.href = '/dashboard.html';
            }, 1500);
        } else {
            showError(data.message || 'Falha ao autenticar');
            btn.disabled = false;
            btn.innerHTML = '<span class="btn-text">Entrar no Portal</span><span class="btn-icon">→</span>';
        }
    })
    .catch(error => {
        console.error('Erro de login:', error);
        let errorMessage = 'Erro ao comunicar com servidor';
        
        // Parse error messages
        if (error.message.includes('Unauthorized')) {
            errorMessage = 'Utilizador ou password inválidos';
        } else if (error.message.includes('Bad Request')) {
            errorMessage = 'Dados inválidos';
        } else if (error.message) {
            errorMessage = error.message;
        }
        
        showError(errorMessage);
        btn.disabled = false;
        btn.innerHTML = '<span class="btn-text">Entrar no Portal</span><span class="btn-icon">→</span>';
    });
}

// ===== ERROR NOTIFICATION ===== (keep existing code)
function showError(message) {
    const notification = createNotification(message, 'error');
    document.body.appendChild(notification);

    setTimeout(() => {
        notification.classList.add('fade-out');
        setTimeout(() => notification.remove(), 300);
    }, 4000);
}

// ===== SUCCESS NOTIFICATION ===== (keep existing code)
function showSuccess(message) {
    const notification = createNotification(message, 'success');
    document.body.appendChild(notification);

    setTimeout(() => {
        notification.classList.add('fade-out');
        setTimeout(() => notification.remove(), 300);
    }, 4000);
}

// ===== CREATE NOTIFICATION ===== (keep existing code)
function createNotification(message, type) {
    // ... keep existing implementation ...
    const notification = document.createElement('div');
    notification.className = `notification notification-${type}`;
    notification.innerHTML = `
        <span class="notification-icon">${type === 'error' ? '❌' : '✅'}</span>
        <span class="notification-message">${message}</span>
    `;

    if (!document.querySelector('style[data-notification]')) {
        const style = document.createElement('style');
        style.setAttribute('data-notification', 'true');
        style.innerHTML = `
            .notification {
                position: fixed;
                top: 20px;
                right: 20px;
                background: rgba(21, 26, 43, 0.95);
                border: 1px solid rgba(0, 255, 156, 0.3);
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
                border-color: rgba(255, 0, 110, 0.3);
                color: #ff006e;
            }

            .notification-success {
                border-color: rgba(0, 255, 156, 0.3);
                color: #00FF9C;
            }

            .notification-icon {
                font-size: 16px;
            }

            .fade-out {
                animation: slideOutRight 0.3s ease-out !important;
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

            @media (max-width: 480px) {
                .notification {
                    left: 10px;
                    right: 10px;
                }
            }
        `;
        document.head.appendChild(style);
    }

    return notification;
}

// ===== FORM INTERACTIONS =====
document.addEventListener('DOMContentLoaded', function() {
    const usernameInput = document.getElementById('username');
    const passwordInput = document.getElementById('password');
    
    [usernameInput, passwordInput].forEach(input => {
        input.addEventListener('keypress', function(e) {
            if (e.key === 'Enter') {
                document.getElementById('loginForm').dispatchEvent(new Event('submit'));
            }
        });
    });

    const inputs = document.querySelectorAll('.form-input');
    inputs.forEach(input => {
        input.addEventListener('focus', function() {
            this.parentElement.classList.add('active');
        });
        input.addEventListener('blur', function() {
            if (!this.value) {
                this.parentElement.classList.remove('active');
            }
        });
    });
});

// ===== MOBILE MENU =====
document.addEventListener('DOMContentLoaded', function() {
    const hamburger = document.getElementById('hamburger');
    const navLinks = document.getElementById('navLinks');

    if (hamburger) {
        hamburger.addEventListener('click', function() {
            navLinks.classList.toggle('open');
        });

        navLinks.querySelectorAll('a').forEach(link => {
            link.addEventListener('click', () => {
                navLinks.classList.remove('open');
            });
        });
    }
});