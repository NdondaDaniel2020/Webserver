# Análise de Falhas na Implementação do Cliente (23/03/2026)

## 📊 Resumo de Status

| # | Falha | Status | Severidade | Impacto |
|---|-------|--------|-----------|---------|
| 1 | reset() não limpa estado CGI | ✅ RESOLVIDO | 🟢 OK | Flags zeradas em keep-alive |
| 2 | sendData() não diferencia EAGAIN | ❌ Ainda existe | � Crítico | Desconecta clientes lentos |
| 3 | cgi.output sem limite | ❌ Ainda existe | 🔴 Crítico | Memory DoS |
| 4 | Copy constructor/assignment (default) | ✅ RESOLVIDO | 🟢 OK | Copias desabilitadas |
| 5 | Destrutor não valida CGI cleanup | ✅ RESOLVIDO | 🟢 OK | Cleanup defensivo implementado |
| 6 | Child herda FDs | ✅ RESOLVIDO | 🟢 OK | CLOEXEC protege |
| 7 | startCgi pipes cleanup | ✅ RESOLVIDO | 🟢 OK | Todos error paths |
| 8 | epoll_ctl error handling | ✅ RESOLVIDO | 🟢 OK | Retorno verificado |

---

## 🔴 3 FALHAS CRÍTICAS

### 1. reset() não limpa estado CGI - State Leakage em Keep-Alive

**Localização:** [Client.cpp línea 427-450](Client.cpp#L427-L450)  
**Status:** ✅ **RESOLVIDO** (23/03/2026)

**Implementation:**
```cpp
void Client::reset()
{
    recv_buffer.clear();
    send_buffer.clear();
    send_offset = 0;

    if (response) {
        delete response;
        response = NULL;
    }

    request = HttpRequest();
    content_length = 0;
    headers_end_pos = 0;

    // ✅ IMPLEMENTADO: Limpar estado CGI e chunked
    is_chunked = false;
    is_cgi_active = false;
    cgi = CgiState();  // Reset para estado inicial

    state = READING_HEADERS;
    updateLastActivity();

    std::cout << "[CLIENT " << fd << "] Reset para keep-alive (is_cgi_active=false, is_chunked=false)" << std::endl;
}
```

**O que foi corrigido:**
- ✅ Flags CGI (`is_cgi_active`, `is_chunked`) agora são zeradas
- ✅ Estrutura `cgi` é resetada para estado inicial
- ✅ Log informativo mostra o reset de flags
- ✅ Nenhuma flag vaza para próxima requisição

**Nota de Segurança:**
Este método assume que `cleanupCgiIfActive()` já foi chamado antes (em `finishCgiAndGenerateResponse()`). O `reset()` apenas limpa estruturas residuais para reutilizar a conexão.

---

### 2. sendData() não diferencia EAGAIN de erro fatal - Desconecta clientes lentos

**Localização:** [Client.cpp línea 387-425](Client.cpp#L387-L425)  
**Status:** ❌ **AINDA EXISTE**

```cpp
bool Client::sendData()
{
    ssize_t sent = write(fd, send_buffer.c_str() + send_offset, to_send);

    if (sent < 0) {
        // ❌ NÃO verifica errno!
        std::cerr << "[CLIENT " << fd << "] Erro ao enviar dados" << std::endl;
        return false;  // Desconecta para QUALQUER erro
    }
    // ... resto do código ...
}
```

**Problema:**
Quando `write()` retorna `-1`, há múltiplas causas:
- `EAGAIN` → Buffer TCP cheio (retry depois)
- `EPIPE` → Cliente fechou (desconectar)
- `ECONNRESET` → Erro real (desconectar)

Código trata TODOS como erro fatal → **desconecta clientes lentos!**

**Cenário:**
- Cliente lento recebe 10MB via conexão 1KB/s
- Após enviar 64KB, write() retorna EAGAIN
- Servidor desconecta abruptamente
- RFC 7231 violation: servidor deve suportar clientes arbitrariamente lentos

**Solução:**
```cpp
if (sent < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // Buffer TCP cheio, retry depois no próximo EPOLLOUT
        std::cout << "[CLIENT " << fd << "] write() EAGAIN - retrying later" << std::endl;
        return false;  // Não é erro crítico
    }

    // Erro real: desconectar
    std::cerr << "[CLIENT " << fd << "] write() erro: " << strerror(errno) << std::endl;
    return false;
}
```

---

### 3. cgi.output sem limite de tamanho - Memory DoS

**Localização:** [Client.cpp línea 705-735](Client.cpp#L705-L735)  
**Status:** ❌ **AINDA EXISTE**

```cpp
void Client::handleCgiStdoutReadable(int epoll_fd)
{
    char buf[8192];
    ssize_t r;
    while ((r = read(cgi.pipe_out[0], buf, sizeof(buf))) > 0) {
        // ❌ Crescimento ilimitado
        cgi.output.append(buf, r);
    }
}
```

**Problema:**
- Script CGI malicioso pode enviar gigabytes de dados
- `cgi.output` cresce infinitamente sem proteção
- Uma simples requisição CGI puede crashar todo servidor

**Ataque:**
```php
<?php while (true) echo "A"; ?>
```
Resultado: `cgi.output` cresce 1MB → 10MB → 100MB → 1GB → OOM killer mata servidor

**Solução:**
```cpp
static const size_t MAX_CGI_OUTPUT_SIZE = 10 * 1024 * 1024;  // 10MB

void Client::handleCgiStdoutReadable(int epoll_fd)
{
    char buf[8192];
    ssize_t r;
    while ((r = read(cgi.pipe_out[0], buf, sizeof(buf))) > 0) {
        // ✅ Validar limite
        if (cgi.output.size() + r > MAX_CGI_OUTPUT_SIZE) {
            std::cerr << "[CGI] Output size limit exceeded" << std::endl;
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, cgi.pipe_out[0], NULL);
            close(cgi.pipe_out[0]);
            cgi.pipe_out[0] = -1;
            kill(cgi.pid, SIGKILL);
            waitpid(cgi.pid, NULL, WNOHANG);
            is_cgi_active = false;

            // Retornar erro 502
            std::string error_msg;
            StatusCodes::http502BadGateway(error_msg, request, "CGI output exceeded limit", server_config);
            send_buffer = error_msg;
            state = SENDING_RESPONSE;
            return;
        }
        cgi.output.append(buf, r);
    }
}
```

---

## 🟠 2 FALHAS ALTAS

### 4. Copy constructor/assignment operator (implícito) com shared resources

**Localização:** [Client.hpp línea 95-97](include/Client.hpp#L95-L97)  
**Status:** ✅ **RESOLVIDO** (23/03/2026)

**Implementation:**
```cpp
// Client.hpp - private section
private:
    // ✅ Desabilitar copy constructor e assignment operator (C++98)
    // Código usa map<int, Client*> (ponteiros), nunca copia objetos
    Client(const Client& other);                    // Not implemented
    Client& operator=(const Client& other);         // Not implemented
```

**O que foi corrigido:**
- ✅ Copy constructor desabilitado (não implementado, privado)
- ✅ Assignment operator desabilitado (não implementado, privado)
- ✅ Compilador gera erro se alguém tentar copiar Client
- ✅ Impossível shallow copy de recursos compartilhados
- ✅ Verificado: Nenhum código no projeto tenta copiar Client

**Verificação:**
- ✅ Tree dos clientes: `map<int, Client*>` (sempre ponteiros)
- ✅ Criaçao: `new Client()` 
- ✅ Referência: Sempre via ponteiro ou iterador
- ✅ Compilação: ✅ Sem erros ("webserv compiled successfully!")

**Benefícios:**
1. **Type safety:** Compilador impede uso acidental de cópias
2. **Resource safety:** Impossível shallow copy de pid/FDs
3. **Clear intent:** Documentação implícita: "Não copia-se Clients"
4. **Zero cost:** Sem overhead, apenas lint restriction

**Nota de Segurança:**
Antes dessa correção, se alguém fizesse:
```cpp
Client copy = original;  // ← COMPILAVA!
// copy.cgi.pid aponta para MESMA pid de original
// Se original.cleanup(), copy.cgi.pid não é atualizado
// Pode levar a double-kill, FD corruption, etc.
```
Agora: **Erro de compilação. Impossível fazer copy.**

---

### 5. Destrutor não valida se CGI foi limpo - Defensive programming

**Localização:** [Client.cpp línea 38-70](Client.cpp#L38-L70)  
**Status:** ✅ **RESOLVIDO** (23/03/2026)

**Implementation:**
```cpp
Client::~Client()
{
    if (response)
        delete response;

    // ✅ RAII: Cliente limpa SEUS recursos (não depende de Server)
    // Defensive programming: Se Server esquecer cleanupCgiIfActive(), não vaza
    
    if (cgi.pid > 0)
    {
        std::cerr << "[WARNING] ~Client() cleaning up active CGI (pid=" << cgi.pid 
                  << "). Server should have called cleanupCgiIfActive()." << std::endl;
        kill(cgi.pid, SIGKILL);
        waitpid(cgi.pid, NULL, WNOHANG);
        cgi.pid = -1;
    }

    // Fechar pipes (kernel remove do epoll automaticamente ao fechar FD)
    if (cgi.pipe_in[1] >= 0)
    {
        close(cgi.pipe_in[1]);
        cgi.pipe_in[1] = -1;
    }

    if (cgi.pipe_out[0] >= 0)
    {
        close(cgi.pipe_out[0]);
        cgi.pipe_out[0] = -1;
    }

    // Zerar flags
    is_cgi_active = false;
    is_chunked = false;
}
```

**O que foi corrigido:**
- ✅ Destrutor implementa **RAII completo** (destrói todos os recursos do Cliente)
- ✅ **Cleanup defensivo:** Mata processo se ainda ativo
- ✅ **Fechamento de pipes** com redundância segura
- ✅ **Zeramento de flags** para evitar estado inconsistente
- ✅ **Warning informativo** se Server esquecer de fazer cleanup

**Mudanças Complementares em Server:**
- ✅ Removido `cleanupCgiIfActive()` de `Server::cleanup()` 
- ✅ Server agora apenas cuida de suas estruturas (epoll, socket, mapa)
- ✅ Divisão clara de responsabilidades: Cliente limpa Cliente, Server limpa Server

**Benefícios:**
1. **RAII Principle:** Destrutor realmente destrói
2. **Separação clara:** Cada classe limpa seus recursos
3. **Edge case protection:** Seguro mesmo com bugs em Server
4. **Redundância segura:** `close(FD=-1)` é no-op kernel
5. **C++ best practices:** Segue padrões modernos

---

## ✅ FALHAS RESOLVIDAS

### 6. Child process herda file descriptors herdados - ✅ RESOLVIDO

**Status:** CLOEXEC implementado corretamente em todos os FDs

### 7. startCgi não fecha pipes em error paths - ✅ RESOLVIDO

**Status:** Todos os 6+ error paths têm cleanup apropriado

### 8. epoll_ctl() error handling - ✅ RESOLVIDO

**Status:** Retorno verificado em ambos `EPOLL_CTL_ADD`

---

## 🟡 OUTRAS FALHAS (Média/Baixa prioridade)

### 9. Content-Length parsing não valida overflow
- Sem validação de formato (negative wrap-around)
- Usar `strtoull()` com verificação de `ERANGE`

### 10. recv_buffer sem limite - Memory DoS
- Headers infinitos podem esgotar memória
- Adicionar `MAX_HEADERS_SIZE = 8KB`

### 11. unchunkBody sem validação - DoS
- Chunk size gigante (FFFFFFFF) não é validado
- Sem limite de tamanho total acumulado
- Adicionar validação de overflow e limites

### 12. Sem validação de método HTTP
- HACK, LOLCAT, etc. são aceitos
- Validar contra whitelist: GET, POST, DELETE, PUT, HEAD, OPTIONS

### 13. const_cast em argv - UB Técnico
- `execve()` espera `char *const argv[]`
- Usar `strdup()` para cópias mutáveis

---

## 📋 RECOMENDAÇÕES

### Prioridade 1 (CRÍTICO - Fazer agora):
1. ❌ Fix `reset()` deixar clean
2. ❌ Fix `sendData()` diferenciar EAGAIN
3. ❌ Fix `cgi.output` limitar tamanho

### Prioridade 2 (ALTO - Esta semana):
4. ❌ Desabilitar copy constructor
5. ⚠️  Adicionar warning no destrutor
6. ❌ Limite de recv_buffer (8KB)
7. ❌ Validação de chunk size

### Prioridade 3 (MÉDIO - Próximo sprint):
8. ❌ Content-Length overflow check
9. ❌ Método HTTP whitelist
10. ❌ Evitar const_cast

---

## 📚 Código relevante

- [Client.cpp](Client.cpp) - Implementação (787+ linhas)
- [Client.hpp](include/Client.hpp) - Definição de classe
- [Server.cpp](src/server/Server.cpp) - closeClient() management

**Last Update:** 23/03/2026
