# 📋 PROBLEMAS NÃO FIXADOS - Validados contra Subject

**Data:** 27 de Março de 2026  
**Alinhamento:** Subject + Guia de Avaliação Oficial (42 Luanda)  
**Fixados:** 7 (SIGPIPE, SIGCHLD, sendData EAGAIN, sendTimeoutResponse, readFile limit, cgi.output limit, recv_buffer limit)  
**Pendentes Válidos:** 16 problemas (removidos 8 que não violam Subject)  
**Total:** 23 problemas (7 fixados + 16 pendentes)

---

## 📋 TABELA RESUMIDA - PROBLEMAS VIOLANDO O SUBJECT

| # | Problema | Arquivo | Severity | Status | Fix Time | Dificuldade | Violação do Subject |
|---|----------|---------|----------|--------|----------|-------------|---------------------|
| 1 | STDERR Corrupted Headers | Client.cpp:640 | 🔴 CRÍTICO | ❌ BROKEN | 5 min | ⭐ | CFG: Output CGI malformado |
| 2 | Path Traversal Upload | ResponseMultipart.cpp:65 | 🔴 CRÍTICO | ❌ BROKEN | 20 min | ⭐⭐ | Segurança: Arbitrary write |
| 3 | TOCTOU Race DELETE | HttpMethods.cpp:105 | 🔴 CRÍTICO | ❌ BROKEN | 25 min | ⭐⭐⭐ | 4.2: Resiliência |
| 4 | CGI Output Pipe Race | Server.cpp:217 | 🔴 CRÍTICO | ⚠️ RISK | 15 min | ⭐⭐ | 4.2: Undefined behavior |
| 5 | handleCgiStdinWritable - No Kill | Client.cpp:798 | 🟠 HIGH | ⚠️ RISK | 10 min | ⭐ | 4.1: "Never freeze" |
| 6 | cgi_fd_map Race Condition | Client.cpp:914 | 🟠 HIGH | ⚠️ RISK | 10 min | ⭐ | 4.2: Resiliência |
| 7 | Symlink Following GET | HttpMethods.cpp:44 | 🟠 HIGH | ❌ BROKEN | 20 min | ⭐⭐ | 5.1: Segurança |
| 8 | Symlink Following CGI | Client.cpp:590 | 🟠 HIGH | ⚠️ RISK | 15 min | ⭐⭐ | 5.1: Segurança |
| 9 | JSON Injection Multipart | ResponseMultipart.cpp:83 | 🟠 HIGH | ⚠️ RISK | 10 min | ⭐ | 5.1: Segurança |
| 10 | No File Count Limit | ResponseMultipart.cpp:21 | 🟠 HIGH | ❌ BROKEN | 15 min | ⭐ | 4.2: DoS / Resiliência |
| 11 | MAX_CLIENTS Unlimited | ServerConnections.cpp | 🟠 HIGH | ❌ BROKEN | 20 min | ⭐⭐ | 4.2: OOM / Resiliência |
| 12 | CGI Headers Parsing Frágil | Client.cpp:880 | 🟡 MEDIUM | ⚠️ RISK | 10 min | ⭐ | 5.1: HTTP correctness |
| 13 | No Max Header Size | Client.cpp:165 | 🟡 MEDIUM | ⚠️ RISK | 5 min | ⭐ | 4.1: Slowloris attack |
| 14 | Double cleanupCgi | Client.cpp:74 | 🟡 MEDIUM | ⚠️ RISK | 10 min | ⭐ | 4.2: Resource leak |
| 15 | Iterator Revalidation Incomplete | ServerEventLoop.cpp:30 | 🟡 MEDIUM | ⚠️ RISK | 15 min | ⭐⭐ | 4.2: Undefined behavior |
| 16 | Double closeClient Risk | ServerConnections.cpp | 🟡 MEDIUM | ⚠️ RISK | 15 min | ⭐ | 4.2: Crash risk |

**Removidos (não violam Subject):**
- ❌ Keep-Alive Case Sensitive (minor, navegadores já tolerantes)
- ❌ JSON Output Not Escaped (não é requisito do Subject)
- ❌ Chunked Encoding O(n²) (é performance, Subject não especifica)
- ❌ Error Messages Disclose Paths (security issue but not Subject violation)
- ❌ checkTimeout O(n) Inefficiency (performance pura)
- ❌ epoll_wait 1000ms Latency (trade-off de design, não violação)
- ❌ CGI Timeout Hardcoded 60s (é configurável ou um padrão aceitável)
- ❌ No SO_KEEPALIVE (LOW priority, não viola Subject)

**Legenda:**
- 🔴 CRÍTICO: Falha direta do Subject → avaliação interrompida
- 🟠 HIGH: Problema de segurança/funcionalidade obrigatória
- 🟡 MEDIUM: Comportamento incorreto edge case

---

## 🔴 PROBLEMAS CRÍTICOS (5 restantes)

### 1. STDERR Corrupted - Quebra Headers HTTP

**Localização:** [src/server/Client.cpp:640](src/server/Client.cpp#L640)  
**Severity:** 🔴 CRÍTICO  
**Status:** ❌ BROKEN

**Problema:**
```cpp
// Client.cpp - startCgi()
if (pid == 0) {  // Child process
    dup2(cgi.pipe_in[0], STDIN_FILENO);
    dup2(cgi.pipe_out[1], STDOUT_FILENO);
    dup2(cgi.pipe_out[1], STDERR_FILENO);  // ← ERRO!
    // Ambos STDOUT e STDERR → mesmo pipe
}
```

**Por que é problema:**
- CGI escreve warning ou erro em STDERR
- STDERR mistura com STDOUT nos headers HTTP
- Headers corrompem, cliente recebe lixo
- Resposta malformada = HTTP 500

**Exemplo:**
```
[php_warning_line]
HTTP/1.1 200 OK
[stderr_warning_line]
Content-Type: text/html
[more_warnings]
```

**Impacto:**
- CGI com warnings = quebrado
- Production scripts com debug output = falham
- Resposta HTTP malformada

**Fix Time:** 5 minutos  
**Difficulty:** ⭐ Fácil

**Solução:**
```cpp
if (pid == 0) {
    dup2(cgi.pipe_in[0], STDIN_FILENO);
    dup2(cgi.pipe_out[1], STDOUT_FILENO);
    dup2(open("/dev/null", O_WRONLY), STDERR_FILENO);  // ← Discard stderr
    // Ou redirecionar para arquivo de log
}
```

---

### 2. CGI Timeout Hardcoded 60 segundos

**Localização:** [src/server/Server.cpp:23](src/server/Server.cpp#L23)  
**Severity:** 🔴 CRÍTICO  
**Status:** ❌ BROKEN

**Problema:**
```cpp
// Server.cpp
#define CGI_TIMEOUT_SECONDS 60

void Server::checkTimeout() {
    if (now - client->getCgiStartTime() > CGI_TIMEOUT_SECONDS) {
        // Kill CGI e enviar 504
        client->finishCgiAndGenerateResponse();
    }
}
```

**Por que é problema:**
- Timeout fixo em 60 segundos
- Scripts legítimos que demoram 90s = abortados
- Upload grande = abortado antes de terminar
- Heavy processing = sempre timeout

**Cenários que falham:**
- Export de database (pode levar 2 minutos)
- Image processing (crop/resize)
- Machine learning inference (segundos)
- File compression

**Impacto:**
- CGI com processamento pesado sempre falha com 504
- Clientes recebem timeout em scripts válidos
- Impossível fazer operações lentas mas legítimas

**Fix Time:** 30 minutos  
**Difficulty:** ⭐⭐ Médio

**Solução:**
- Adicionar `timeout` configurável no arquivo de config
- Default 60s, mas client pode aumentar
- Validar range (5-3600 segundos)

**Exemplo config:**
```
server {
    listen 8080;
    server_name localhost;
    cgi_timeout 120;  // Novo parâmetro
}
```

---

### 3. Path Traversal em Upload (Multipart)

**Localização:** [src/http/ResponseMultipart.cpp:65](src/http/ResponseMultipart.cpp#L65)  
**Severity:** 🔴 CRÍTICO  
**Status:** ❌ BROKEN

**Problema:**
```cpp
// ResponseMultipart.cpp - parseMultipartForm()
std::string filename = extractFilename(headers);
std::string filepath = upload_dir + "/" + filename;
// ← SEM VALIDAÇÃO!

std::ofstream file(filepath.c_str());
file.write(data.c_str(), data.size());
```

**Ataque:**
```
POST /upload HTTP/1.1
Content-Type: multipart/form-data; boundary=----

------
Content-Disposition: form-data; name="file"; filename="../../../etc/passwd"
...binary data...
------
```

**Resultado:**
- Arquivo criado em `/etc/passwd` (!)
- Overwrite de arquivos do sistema
- Possível RCE se sobrescrever script web

**Impacto:**
- 🔴 Arbitrary file write
- System compromise
- Integrity violation

**Fix Time:** 20 minutos  
**Difficulty:** ⭐⭐ Médio

**Solução:**
```cpp
// Validar path
std::string sanitized_filename = basename(filename);  // Remove path
// Rejeitar se contiver:
if (filename.find("..") != string::npos ||
    filename.find("/") != string::npos) {
    return 400;  // Bad request
}
```

---

### 4. TOCTOU Race Condition em DELETE

**Localização:** [src/http/HttpMethods.cpp:105-132](src/http/HttpMethods.cpp#L105-L132)  
**Severity:** 🔴 CRÍTICO  
**Status:** ❌ BROKEN

**Problema:**
```cpp
// HttpMethods.cpp - methodDelete()
// Time-of-check vs Time-of-use race

// T1: Cliente A
if (fileExists(path)) {  // ← CHECK: arquivo existe
    // ... context switch ...
    
    // T2: Cliente B cria arquivo em path
    
    // T1 resume
    deleteFile(path);  // ← USE: deleta arquivo de B!
}
```

**Cenário de ataque:**
```
Timeline:
T1: Cliente A: DELETE /uploads/important.txt
    → Verifica que existe ✓
T2: Cliente B: POST /uploads/important.txt
    → Cria novo arquivo
T1: Cliente A: prossegue DELETE
    → Deleta arquivo de B!
```

**Impacto:**
- 🔴 Arbitrary file deletion
- Dados de outro cliente apagados
- Possible DoS (delete assets)

**Fix Time:** 25 minutos  
**Difficulty:** ⭐⭐⭐ Difícil

**Solução:**
- Usar atomic operations (rename atomically)
- Ou usar file locks
- Ou validar ownership antes de deletar

```cpp
// Versão segura
int fd = open(path, O_WRONLY | O_CREAT | O_EXCL);
if (fd < 0) return 409;  // Conflict - arquivo já existe
unlink(path);  // Agora seguro
```

---

### 5. CGI Output Pipe Race Condition

**Localização:** [src/server/Server.cpp:217](src/server/Server.cpp#L217)  
**Severity:** 🔴 CRÍTICO  
**Status:** ⚠️ RISK

**Problema:**
```cpp
// Server.cpp - handleCgiPipeEvent()
void Server::handleCgiPipeEvent(Client *c, int events_mask) {
    if (events_mask & EPOLLIN) {
        c->handleCgiStdoutReadable(epoll_fd);  // ← FECHA PIPE AQUI
    }
    
    if (events_mask & EPOLLHUP) {  // ← PIPE JÁ PODE ESTAR FECHADO!
        c->finishCgiAndGenerateResponse();
    }
}

// Client.cpp - handleCgiStdoutReadable()
ssize_t r = read(cgi.pipe_out[0], buf, sizeof(buf));
if (r == 0) {  // EOF
    close(cgi.pipe_out[0]);
    cgi.pipe_out[0] = -1;  // ← Seta -1
}
```

**Race:**
1. EPOLLIN dispara, lê dados, acha EOF
2. Fecha pipe, seta `cgi.pipe_out[0] = -1`
3. Epoll ainda tem event EPOLLHUP pendente
4. EPOLLHUP event processa pipe inválido (-1)
5. Possível corruption

**Impacto:**
- ⚠️ Undefined behavior
- Possible use-after-free
- Corruption de state

**Fix Time:** 15 minutos  
**Difficulty:** ⭐⭐ Médio

**Solução:**
```cpp
// Adicionar flag para evitar duplicate processing
bool cgi_closed = false;

if (r == 0) {
    close(cgi.pipe_out[0]);
    cgi.pipe_out[0] = -1;
    cgi_closed = true;
}

// Depois, em handleCgiPipeEvent:
if (events_mask & EPOLLHUP && !cgi_closed) {
    // Process EPOLLHUP
}
```

---

## 🟠 PROBLEMAS HIGH (7 problemas)

### 6. handleCgiStdinWritable Não Mata CGI em Erro

**Localização:** [src/server/Client.cpp:798-810](src/server/Client.cpp#L798-L810)  
**Severity:** 🟠 HIGH  
**Status:** ⚠️ RISK

**Problema:**
```cpp
void Client::handleCgiStdinWritable(int epoll_fd) {
    ssize_t w = write(cgi.pipe_in[1], ...);
    
    if (w < 0) {
        if (errno == EAGAIN) return;  // Retry
        
        // Erro real
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, cgi.pipe_in[1], NULL);
        close(cgi.pipe_in[1]);
        // ← MAS não mata CGI!
        // ← MAS não fecha stdout pipe!
    }
}
```

**Consequência:**
- CGI ainda rodando, esperando STDIN
- STDOUT pipe aberto mas nunca vai escrever
- Cliente pendurado esperando resposta
- Timeout 60s até desistir

**Impacto:**
- 🟠 Cliente fica pendurado por 60 segundos
- Recurso desperdiçado

**Fix Time:** 10 minutos  
**Difficulty:** ⭐ Fácil

---

### 7. cgi_fd_map Race Condition

**Localização:** [src/server/Client.cpp:914](src/server/Client.cpp#L914)  
**Severity:** 🟠 HIGH  
**Status:** ⚠️ RISK

**Problema:**
- Pipe adicionado ao epoll antes de ser registrado no mapa
- Output pode ser lido e descartado antes de estar no mapa
- Dados perdidos

**Fix Time:** 10 minutos  
**Difficulty:** ⭐ Fácil

---

### 8. Symlink Following em GET

**Localização:** [src/http/HttpMethods.cpp:44](src/http/HttpMethods.cpp#L44)  
**Severity:** 🟠 HIGH  
**Status:** ❌ BROKEN

**Problema:**
```cpp
// HttpMethods.cpp - methodGet()
Response::Response(request, config) {
    // Lê arquivo
    content = readFile(filepath);  // ← SEM validar symlinks!
}
```

**Ataque:**
```bash
# No servidor
ln -s /etc/passwd www/secret.txt

# Cliente
curl http://localhost:8080/secret.txt
# Retorna conteúdo de /etc/passwd!
```

**Impacto:**
- 🟠 Information disclosure
- Expõe arquivo confidencial se admin errar config

**Fix Time:** 20 minutos  
**Difficulty:** ⭐⭐ Médio

**Solução:**
```cpp
// Validar que não é symlink
struct stat sb;
lstat(filepath, &sb);  // lstat = não segue symlinks
if (S_ISLNK(sb.st_mode)) {
    return 403;  // Forbidden
}
```

---

### 9. Symlink Following em CGI

**Localização:** [src/server/Client.cpp:590](src/server/Client.cpp#L590)  
**Severity:** 🟠 HIGH  
**Status:** ⚠️ RISK

**Problema:**
```cpp
// Client.cpp - startCgi()
std::string script_path = loc.root + req.getPath().substr(loc.path.size());
// ← Script poderia ser symlink fora de cgi-bin/
```

**Impact:**
- 🟠 Escape from sandbox
- Execute scripts fora de cgi-bin/

**Fix Time:** 15 minutos  
**Difficulty:** ⭐⭐ Médio

---

### 10. Keep-Alive Case Sensitivity

**Localização:** [src/server/Client.cpp:299-303](src/server/Client.cpp#L299-L303)  
**Severity:** 🟠 HIGH  
**Status:** ⚠️ RISK

**Problema:**
```cpp
if (request.hasHeader("Connection")) {
    std::string conn = request.getHeader("Connection");
    keep_alive = (conn == "keep-alive");  // ← Case sensitive!
}
```

**Teste:**
```
GET / HTTP/1.1
Connection: Keep-Alive   (capitalized)
```

**Resultado:**
- keep_alive = false (não matchou)
- Conexão fecha inesperadamente

**Impact:**
- 🟠 Incompatibilidade com alguns clientes
- Clientes legítimos desconectados

**Fix Time:** 5 minutos  
**Difficulty:** ⭐ Fácil

---

### 11. JSON Injection em Multipart

**Localização:** [src/http/ResponseMultipart.cpp:83](src/http/ResponseMultipart.cpp#L83)  
**Severity:** 🟠 HIGH  
**Status:** ⚠️ RISK

**Problema:**
```cpp
// ResponseMultipart.cpp
json_response += "\"filename\": \"" + filename + "\"";
// ← SEM ESCAPE!
```

**Ataque:**
```
filename: test".txt", "evil": "value
```

**Resultado:**
```json
"filename": "test".txt", "evil": "value"
// ↑ JSON inválido/injection
```

**Impact:**
- 🟠 JSON corruption
- Possível client-side code execution

**Fix Time:** 10 minutos  
**Difficulty:** ⭐ Fácil

---

### 12. No Upload File Count Limit

**Localização:** [src/http/ResponseMultipart.cpp:21](src/http/ResponseMultipart.cpp#L21)  
**Severity:** 🟠 HIGH  
**Status:** ❌ BROKEN

**Problema:**
```cpp
// Sem limite

// Pode receber: 100,000 arquivos em um POST
for (int i = 0; i < 100000; i++) {
    // Cria arquivo
}
```

**Impacto:**
- 🟠 DoS - esgota inodes do filesystem
- Filesystem cheio
- Nenhum outro serviço consegue criar arquivo

**Solução:**
- Adicionar limite (ex: 100 files por request)
- Validar quando parsing multipart

**Fix Time:** 15 minutos  
**Difficulty:** ⭐ Fácil

---

### 13. MAX_CLIENTS Unlimited

**Localização:** [src/server/ServerConnections.cpp](src/server/ServerConnections.cpp)  
**Severity:** 🟠 HIGH  
**Status:** ❌ BROKEN

**Problema:**
```cpp
// Sem limite de conexões simultâneas

// Ataque:
for (int i = 0; i < 100000; i++) {
    connect(sock, "localhost:8080")
}
```

**Resultado:**
- RAM esgota (cada Client = ~1-10KB)
- 100k clientes = 1GB+ RAM
- OOM killer mata servidor

**Impacto:**
- 🔴 DoS crítico
- Servidor morre

**Fix Time:** 20 minutos  
**Difficulty:** ⭐⭐ Médio

**Solução:**
```cpp
#define MAX_CLIENTS 1000  // Limit simultaneous connections

if (clients.size() >= MAX_CLIENTS) {
    close(new_fd);  // Reject
    return;
}
```

---

## 🟡 PROBLEMAS MEDIUM (10 problemas)

### 14. CGI Headers Parsing Frágil

**Localização:** [src/server/Client.cpp:880-910](src/server/Client.cpp#L880-L910)  
**Severity:** 🟡 MEDIUM  
**Status:** ⚠️ RISK

**Problema:**
```cpp
if (line.find("Status: ") == 0) {
    status_line = line.substr(8);
    // ← Aceita "Status: " sem número!
}
```

**Teste:**
```
Status:
```

**Resultado:**
- status_line vazio
- Invalid HTTP response

**Fix Time:** 10 minutos  
**Difficulty:** ⭐ Fácil

---

### 15. No Max Header Size

**Localização:** [src/server/Client.cpp:165-168](src/server/Client.cpp#L165-L168)  
**Severity:** 🟡 MEDIUM  
**Status:** ⚠️ RISK

**Problema:**
- Sem limite de tamanho de headers
- Slowloris header attack possível
- Cliente envia gigabyte de headers lentamente

**Impact:**
- 🟡 Memory leak (buffer cresce)
- Possible DoS

**Fix Time:** 5 minutos  
**Difficulty:** ⭐ Fácil

---

### 16. Chunked Encoding O(n²) Parsing

**Localização:** [src/server/Client.cpp:168-186](src/server/Client.cpp#L168-L186)  
**Severity:** 🟡 MEDIUM  
**Status:** ⚠️ RISK

**Problema:**
- String operations ineficientes
- O(n²) para múltiplos chunks
- Lento com uploads grandes

**Impact:**
- 🟡 Performance degradation
- Possível timeout

**Fix Time:** 30 minutos  
**Difficulty:** ⭐⭐⭐ Difícil

---

### 17. Error Messages Disclose Filesystem Paths

**Localização:** [src/http/StatusCodes.cpp](src/http/StatusCodes.cpp)  
**Severity:** 🟡 MEDIUM  
**Status:** ⚠️ RISK

**Problema:**
```
File not found: /var/www/html/secret.txt
CGI script not found: /var/www/cgi-bin/admin.php
```

**Impact:**
- 🟡 Information disclosure
- Attacker aprende estrutura filesystem

**Fix Time:** 15 minutos  
**Difficulty:** ⭐ Fácil

---

### 18. JSON Output Not Escaped

**Localização:** [src/http/ResponseMultipart.cpp:83](src/http/ResponseMultipart.cpp#L83)  
**Severity:** 🟡 MEDIUM  
**Status:** ⚠️ RISK

**Problema:**
- Caracteres especiais não escapados
- Pode quebrar JSON output

**Fix Time:** 10 minutos  
**Difficulty:** ⭐ Fácil

---

### 19. Double cleanupCgi Possible

**Localização:** [src/server/Client.cpp:74](src/server/Client.cpp#L74)  
**Severity:** 🟡 MEDIUM  
**Status:** ⚠️ RISK

**Problema:**
```cpp
// Destrutor chama cleanup
~Client() {
    if (cgi.pid > 0) waitpid(cgi.pid);  // Cleanup #1
}

// Mas também pode ser chamoado explicitamente
cleanupCgiIfActive(epoll_fd);  // Cleanup #2
```

**Impact:**
- 🟡 Double free risk
- Undefined behavior

**Fix Time:** 10 minutos  
**Difficulty:** ⭐ Fácil

---

### 20. checkTimeout O(n) Inefficiency

**Localização:** [src/server/ServerConnections.cpp:130](src/server/ServerConnections.cpp#L130)  
**Severity:** 🟡 MEDIUM  
**Status:** ❌ BROKEN

**Problema:**
```cpp
for (std::map<int, Client*>::iterator it = clients.begin(); ...) {
    // Check timeout para CADA cliente
    // O(n) executado a cada segundo!
}
```

**Com 10k clientes:**
- 10k iterações / segundo
- CPU busy

**Impact:**
- 🟡 Performance degradation
- High CPU usage

**Fix Time:** 20 minutos  
**Difficulty:** ⭐⭐ Médio

---

### 21. epoll_wait 1000ms High Latency

**Localização:** [src/server/ServerEventLoop.cpp:11](src/server/ServerEventLoop.cpp#L11)  
**Severity:** 🟡 MEDIUM  
**Status:** ❌ BROKEN

**Problema:**
```cpp
int n = epoll_wait(epoll_fd, events, 64, 1000);  // 1 segundo timeout
```

**Impact:**
- 🟡 1 segundo latência mínima
- Se cliente desconecta sem dados, demora 1s para detectar

**Fix Time:** 5 minutos  
**Difficulty:** ⭐ Fácil

---

### 22. Iterator Revalidation Incomplete

**Localização:** [src/server/ServerEventLoop.cpp:30](src/server/ServerEventLoop.cpp#L30)  
**Severity:** 🟡 MEDIUM  
**Status:** ⚠️ RISK

**Problema:**
- EPOLLOUT processado mesmo após EPOLLIN fecha cliente
- Undefined behavior possível

**Fix Time:** 15 minutos  
**Difficulty:** ⭐⭐ Médio

---

### 23. Double closeClient Risk

**Localização:** [src/server/ServerConnections.cpp](src/server/ServerConnections.cpp)  
**Severity:** 🟡 MEDIUM  
**Status:** ⚠️ RISK

**Problema:**
- closeClient() pode ser chamado 2x em shutdown
- Double cleanup de recursos

**Impact:**
- 🟡 Undefined behavior
- Possible crash no shutdown

**Fix Time:** 15 minutos  
**Difficulty:** ⭐ Fácil

---

## 🟢 PROBLEMAS LOW (2 problemas)

### 24. No SO_KEEPALIVE on Sockets

**Localização:** [src/server/ServerConnections.cpp](src/server/ServerConnections.cpp)  
**Severity:** 🟢 LOW  
**Status:** ❌ BROKEN

**Problema:**
```cpp
// Socket criado SEM SO_KEEPALIVE
// Conexão TCP morta não é detectada
```

**Impact:**
- 🟢 Zombie connections possíveis
- Cliente derreça, socket fica aberto

**Fix Time:** 5 minutos  
**Difficulty:** ⭐ Fácil

---

## 📊 RESUMO POR SEVERITY

| Severity | Count | Broken | At-Risk | Fixed |
|----------|-------|--------|---------|-------|
| 🔴 CRITICAL | 6 | 4 | 1 | 1 |
| 🟠 HIGH | 9 | 4 | 5 | 0 |
| 🟡 MEDIUM | 10 | 3 | 7 | 0 |
| 🟢 LOW | 6 | 2 | 1 | 3 |
| **GRAND TOTAL** | **31** | **13** | **14** | **4** |

---

## 🎯 RECOMENDAÇÃO POr IMPACT

**Top 10 High-Impact Fixes (em ordem):**

1. ✅ **SIGPIPE handler** (DONE)
2. ✅ **SIGCHLD handler** (DONE)
3. ✅ **sendData EAGAIN** (DONE)
4. ✅ **sendTimeoutResponse async** (DONE)
5. ✅ **readFile 10MB limit** (DONE)
6. ✅ **CGI output 10MB limit** (DONE)
7. ✅ **recv_buffer 10MB limit** (DONE)
8. 🔴 **MAX_CLIENTS limit** (20 min) ← DoS prevention
9. 🔴 **Path Traversal fix** (20 min) ← Security critical
10. 🔴 **STDERR redirection** (5 min) ← Quick win

---

## 📈 NOTA ESTIMADA POR IMPLEMENTAÇÃO

```
BASE (compila, features work):         50 points

✅ DONE (7 CRITICAL):                 +40 points
🔴 REMAINING CRITICAL (5 URGENT):     -20 points avg
🟠 REMAINING HIGH (7):                 -15 points avg
🟡 REMAINING MEDIUM (10):              -5 points avg

TOTAL: 50 + 40 - 20 = 70 (PASSA COM 7 FIXOS)
TOTAL: 50 + 40 - 10 = 80 (COM +3 FIXES CRÍTICOS)
TOTAL: 50 + 40 - 5 = 85 (COM +5 FIXES TOP)
```

---

**Próximas ações recomendadas:**
1. Fixar os 5 CRITICAL restantes (prioridade)
2. Depois HIGH severity
3. MEDIUM/LOW conforme tempo permitir

