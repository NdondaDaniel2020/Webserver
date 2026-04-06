# Autenticação com CGI — Documentação

## Scripts Criados

### 1. `signup.py` — Criar Conta
**Endpoint:** `POST /cgi-bin/signup.py`

**Parâmetros (form-urlencoded):**
- `username` (obrigatório): 3-20 caracteres, apenas letras, números e underscore
- `email` (obrigatório): Email válido
- `password` (obrigatório): Mínimo 6 caracteres

**Resposta de Sucesso (201 Created):**
```json
{
  "ok": true,
  "message": "Conta criada com sucesso"
}
```

**Resposta de Erro (400/409):**
```json
{
  "ok": false,
  "error": "Mensagem de erro"
}
```

**Validações:**
- Username já existe: 409 Conflict
- Email já existe: 409 Conflict
- Formato inválido: 400 Bad Request

---

### 2. `login.py` — Fazer Login com Sessão
**Endpoint:** `POST /cgi-bin/login.py`

**Parâmetros (form-urlencoded):**
- `username` (obrigatório): Username registado
- `password` (obrigatório): Password da conta

**Resposta de Sucesso (200 OK):**
```json
{
  "ok": true,
  "message": "Login realizado com sucesso",
  "username": "ninja_master"
}
```
**Header Cookie:**
```
Set-Cookie: session=<token>; Path=/; Max-Age=604800; HttpOnly; SameSize=Lax
```

**Resposta de Erro (401):**
```json
{
  "ok": false,
  "error": "Utilizador ou password inválidos"
}
```

---

### 3. `verify_session.py` — Verificar Sessão
**Endpoint:** `GET /cgi-bin/verify_session.py`

**Request:**
- Cookie: `session=<token>`

**Resposta de Sucesso (200 OK):**
```json
{
  "ok": true,
  "authenticated": true,
  "username": "ninja_master",
  "user_id": 1,
  "expires_at": "2026-04-13 10:30:45"
}
```

**Resposta de Erro (401):**
```json
{
  "ok": false,
  "authenticated": false,
  "error": "Sessão inválida ou expirada"
}
```

---

### 4. `logout.py` — Terminar Sessão
**Endpoint:** `POST /cgi-bin/logout.py`

**Request:**
- Cookie: `session=<token>`

**Resposta de Sucesso (200 OK):**
```json
{
  "ok": true,
  "message": "Logout realizado com sucesso"
}
```
**Header Cookie:**
```
Set-Cookie: session=; Path=/; Max-Age=0
```

---

## Bases de Dados

### `/tmp/users.db`
Tabela `users`:
- `id`: INTEGER PRIMARY KEY
- `username`: TEXT UNIQUE
- `email`: TEXT UNIQUE
- `password_hash`: TEXT (PBKDF2-SHA256 com salt)
- `created_at`: DATETIME

### `/tmp/sessions.db`
Tabela `sessions`:
- `id`: INTEGER PRIMARY KEY
- `session_token`: TEXT UNIQUE
- `user_id`: INTEGER
- `username`: TEXT
- `created_at`: DATETIME
- `expires_at`: DATETIME (7 dias)

---

## Segurança

✅ **Password:** Hash com PBKDF2-SHA256 + salt aleatório  
✅ **Cookie:** HttpOnly + SameSite=Lax (CSRF protection)  
✅ **Sessão:** Expira em 7 dias  
✅ **Validação:** Email e username únicos  

---

## Exemplo de Uso (Cliente)

### Criar Conta
```javascript
fetch('./cgi-bin/signup.py', {
  method: 'POST',
  credentials: 'include',
  headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
  body: new URLSearchParams({
    username: 'ninja_master',
    email: 'ninja@example.com',
    password: '123456'
  }).toString()
})
.then(r => r.json())
.then(data => console.log(data))
```

### Fazer Login
```javascript
fetch('./cgi-bin/login.py', {
  method: 'POST',
  credentials: 'include',
  headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
  body: new URLSearchParams({
    username: 'ninja_master',
    password: '123456'
  }).toString()
})
.then(r => r.json())
.then(data => {
  if (data.ok) {
    console.log('Login bem-sucedido!');
    // Cookie é automaticamente enviado nos próximos requests
  }
})
```

### Verificar Sessão
```javascript
fetch('./cgi-bin/verify_session.py', {
  method: 'GET',
  credentials: 'include'
})
.then(r => r.json())
.then(data => {
  if (data.authenticated) {
    console.log('Utilizador: ' + data.username);
  } else {
    console.log('Não autenticado');
  }
})
```

### Fazer Logout
```javascript
fetch('./cgi-bin/logout.py', {
  method: 'POST',
  credentials: 'include'
})
.then(r => r.json())
.then(data => {
  if (data.ok) {
    console.log('Logout realizado!');
    // Redirecionar para login
  }
})
```

---

## Notas Importantes

1. **CORS**: Os scripts têm `Access-Control-Allow-Origin: *` para testes
2. **Permissões**: Garantir que os scripts têm permissão de execução (755)
3. **Cookies**: Requer `credentials: 'include'` nos fetch requests
4. **UTF-8**: Todos os scripts suportam UTF-8 corretamente
