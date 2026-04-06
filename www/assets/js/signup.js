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
            console.log('❌ Sem sessão válida, permitindo signup');
            // Clear any stale session data
            document.cookie = 'session=; path=/; expires=Thu, 01 Jan 1970 00:00:00 UTC;';
        }
    })
    .catch(error => {
        console.log('Sem sessão válida, permitindo signup:', error);
        // Clear any stale session data
        document.cookie = 'session=; path=/; expires=Thu, 01 Jan 1970 00:00:00 UTC;';
    });
}

// ===== FORM SUBMISSION =====
document.getElementById('signupForm').addEventListener('submit', function(e) {
    e.preventDefault();

    const username = document.getElementById('username').value.trim();
    const email = document.getElementById('email').value.trim();
    const password = document.getElementById('password').value.trim();
    const confirmPassword = document.getElementById('confirmPassword').value.trim();
    const termsAccepted = document.getElementById('terms').checked;

    // Validation
    if (!username || !email || !password || !confirmPassword) {
        showError('Por favor, preenche todos os campos');
        return;
    }

    if (username.length < 3) {
        showError('O username deve ter pelo menos 3 caracteres');
        return;
    }

    if (username.length > 20) {
        showError('O username não pode ter mais de 20 caracteres');
        return;
    }

    if (!isValidEmail(email)) {
        showError('Por favor, insere um email válido');
        return;
    }

    if (password.length < 6) {
        showError('A password deve ter pelo menos 6 caracteres');
        return;
    }

    if (password !== confirmPassword) {
        showError('As passwords não coincidem');
        return;
    }

    if (!termsAccepted) {
        showError('Tens de aceitar os Termos de Serviço');
        return;
    }

    // Submit to server
    submitSignup(username, email, password);
});

// ===== EMAIL VALIDATION =====
function isValidEmail(email) {
    const emailRegex = /^[^\s@]+@[^\s@]+\.[^\s@]+$/;
    return emailRegex.test(email);
}

// ===== FORM SUBMISSION HANDLER =====
function submitSignup(username, email, password) {
    const btn = document.querySelector('.btn-signup');
    btn.disabled = true;
    btn.innerHTML = '<span class="spinner"></span> Criando conta...';

    // Send POST request to server
    fetch('./cgi-bin/signup.py', {
        method: 'POST',
        credentials: 'include',
        headers: {
            'Content-Type': 'application/x-www-form-urlencoded'
        },
        body: new URLSearchParams({
            username: username,
            email: email,
            password: password
        }).toString()
    })
    .then(response => {
        if (!response.ok) {
            return response.json().then(data => {
                throw new Error(data.message || `Erro ${response.status}: ${response.statusText}`);
            }).catch(parseError => {
                throw new Error(`Erro ${response.status}: ${response.statusText}`);
            });
        }
        return response.json();
    })
    .then(data => {
        if (data.ok) {
            console.log('✅ Conta criada com sucesso! Dados recebidos:', data);
            showSuccess('Conta criada com sucesso! Redirecionando para login...');

            // Redirect to login page after delay
            setTimeout(() => {
                window.location.href = './login.html';
            }, 1500);
        } else {
            showError(data.message || 'Falha ao criar conta');
            btn.disabled = false;
            btn.innerHTML = '<span class="btn-text">Criar Conta</span><span class="btn-icon">✓</span>';
        }
    })
    .catch(error => {
        console.error('Erro ao criar conta:', error);
        let errorMessage = 'Erro ao comunicar com servidor';
        
        if (error.message.includes('Conflict')) {
            errorMessage = 'Este username ou email já está registado';
        } else if (error.message.includes('Bad Request')) {
            errorMessage = 'Dados inválidos';
        } else if (error.message) {
            errorMessage = error.message;
        }
        
        showError(errorMessage);
        btn.disabled = false;
        btn.innerHTML = '<span class="btn-text">Criar Conta</span><span class="btn-icon">✓</span>';
    });
}

// ===== ERROR NOTIFICATION =====
function showError(message) {
    const notification = createNotification(message, 'error');
    document.body.appendChild(notification);

    setTimeout(() => {
        notification.classList.add('fade-out');
        setTimeout(() => notification.remove(), 300);
    }, 4000);
}

// ===== SUCCESS NOTIFICATION =====
function showSuccess(message) {
    const notification = createNotification(message, 'success');
    document.body.appendChild(notification);

    setTimeout(() => {
        notification.classList.add('fade-out');
        setTimeout(() => notification.remove(), 300);
    }, 4000);
}

// ===== CREATE NOTIFICATION ELEMENT =====
function createNotification(message, type) {
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

// ===== REAL-TIME VALIDATION =====
// Username validation
document.getElementById('username').addEventListener('input', function() {
    const username = this.value.trim();
    if (username.length > 0 && username.length < 3) {
        this.style.borderColor = 'rgba(255, 0, 110, 0.5)';
    } else if (username.length > 20) {
        this.style.borderColor = 'rgba(255, 0, 110, 0.5)';
    } else {
        this.style.borderColor = '';
    }
});

// Email validation
document.getElementById('email').addEventListener('input', function() {
    const email = this.value.trim();
    if (email.length > 0 && !isValidEmail(email)) {
        this.style.borderColor = 'rgba(255, 0, 110, 0.5)';
    } else {
        this.style.borderColor = '';
    }
});

// Password length validation
document.getElementById('password').addEventListener('input', function() {
    const password = this.value.trim();
    if (password.length > 0 && password.length < 6) {
        this.style.borderColor = 'rgba(255, 0, 110, 0.5)';
    } else {
        this.style.borderColor = '';
    }
});

// Confirm password match validation
document.getElementById('confirmPassword').addEventListener('input', function() {
    const password = document.getElementById('password').value.trim();
    const confirmPassword = this.value.trim();
    
    if (confirmPassword.length > 0 && password !== confirmPassword) {
        this.style.borderColor = 'rgba(255, 0, 110, 0.5)';
    } else {
        this.style.borderColor = '';
    }
});
