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
    fetch('/api/signup', {
        method: 'POST',
        credentials: 'include',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify({
            username: username,
            email: email,
            password: password
        })
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
        if (data.success) {
            console.log('✅ Conta criada com sucesso! Dados recebidos:', data);
            showSuccess('✅ Conta criada com sucesso! Redirecionando para login...');

            // Redirect to login page after delay
            setTimeout(() => {
                window.location.href = './login.html';
            }, 1500);
        } else {
            showError('❌ ' + (data.message || 'Falha ao criar conta'));
            btn.disabled = false;
            btn.innerHTML = '<span class="btn-text">Criar Conta</span><span class="btn-icon">✓</span>';
        }
    })
    .catch(error => {
        console.error('Erro ao criar conta:', error);
        let errorMessage = 'Erro ao comunicar com servidor';
        
        if (error.message.includes('Conflict')) {
            errorMessage = '❌ Este username ou email já está registado';
        } else if (error.message.includes('Bad Request')) {
            errorMessage = '❌ Dados inválidos';
        } else if (error.message) {
            errorMessage = '❌ ' + error.message;
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
    notification.className = `notification ${type}`;
    notification.textContent = message;
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
