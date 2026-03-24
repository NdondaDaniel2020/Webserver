# Análise de Falhas na Implementação CGI

## � Resumo de Status (22/03/2026)

| # | Falha | Status | Prioridade |
|---|-------|--------|-----------|
| 1 | DoS - Sem limite de memória | ❌ Ainda existe | 🔴 Crítico |
| 2 | Ignora status de saída do filho | ❌ Ainda existe | 🔴 Crítico |
| 3 | Race condition no registro de pipes | ❌ Ainda existe | 🟠 Alto |
| 4 | stderr misturado com stdout | ❌ Ainda existe | 🟠 Alto |
| 5 | Erro de write no stdin | ⚠️ Parcial | 🟠 Alto |
| 6 | Timeout CGI (10s hardcoded) | ⚠️ Parcial | 🟠 Alto |
| 7 | Não valida interpretador | ❌ Ainda existe | 🟠 Alto |
| 8 | Parsing de headers frágil | ⚠️ Parcial | 🟡 Médio |
| 9 | Sem detecção de sinais | ❌ Ainda existe | 🟠 Alto |
| 10 | Configuração dependente de ordem | ✅ Resolvido | 🔵 N/A |
| 11 | Responsabilidade duplicada | ✅ Resolvido | 🔵 N/A |

---

## �🔴 FALHAS CRÍTICAS

### 1. **Vulnerabilidade de DoS - Sem limite de memória para output CGI**
**Status:** ❌ **AINDA EXISTE** (linhas 774, 805)
**Localização:** `src/server/Client.cpp:774, 805`

```cpp
cgi.output.append(buf, r);  // ← Crescimento ilimitado de memória
```

**Problema:**
- Script pode escrever infinitamente (ex: `while(1) print("X" * 1000000)`)
- Cada cliente CGI pode consumir GBs de RAM
- Múltiplos clientes simultâneos podem esgotar memória do servidor
- Sem proteção contra OOM killer

**Cenário de Ataque:**
```python
#!/usr/bin/python3
print("Content-Type: text/plain\r\n")
while True:
    print("X" * 1000000)  # 1MB por iteração
```

**Impacto:** Ataque DoS trivial que pode derrubar o servidor

**Solução Recomendada:**
- Adicionar limite de memória (ex: 10MB ou 100MB configurável)
- Abortar CGI com SIGKILL e retornar 413 Payload Too Large se exceder
- Considerar bufferização em arquivo temporário para respostas grandes

---

### 2. **Ignora status de saída do processo filho**
**Status:** ❌ **AINDA EXISTE** (linhas 800-810)
**Localização:** `src/server/Client.cpp:800-810`

```cpp
int status;
pid_t result = waitpid(cgi.pid, &status, WNOHANG);
if (result == cgi.pid)
{
    // ← status não é verificado com WIFEXITED, WEXITSTATUS, WIFSIGNALED
}
```

**Problema:**
- `execve` failure (`exit(127)`) aparece como resposta 200 OK com body vazio
- Crashes (SIGSEGV, SIGKILL, SIGABRT) não são detectados
- Impossível distinguir sucesso de falha de execução

**Cenário:**
1. Configuração: `cgi_path /usr/bin/python99` (não existe)
2. `fork()` sucede, mas `execve()` falha
3. Filho executa `exit(127)`
4. Pai ignora status e gera: `HTTP/1.1 200 OK` com body vazio

**Impacto:** Diagnóstico falho, usuário não vê erros reais

**Solução Recomendada:**
```cpp
int status;
pid_t result = waitpid(cgi.pid, &status, WNOHANG);
if (result == cgi.pid) {
    if (WIFEXITED(status)) {
        int exit_code = WEXITSTATUS(status);
        if (exit_code == 127) {
            // execve falhou
            return_502_error();
        }
    }
    if (WIFSIGNALED(status)) {
        int signal = WTERMSIG(status);
        // Processo morreu por sinal (SIGSEGV, SIGKILL, etc)
        return_502_error();
    }
}
```

---

### 3. **Race condition no registro de pipes**
**Status:** ❌ **AINDA EXISTE** (Server.cpp:297-299)
**Localização:** Flow entre `Client.cpp:710-715` (startCgi epoll_ctl) e `Server.cpp:297-299` (cgi_fd_map[])

**Código Atual:**
```cpp
// Em Client.cpp:710-715
if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, cgi.pipe_out[0], &ev) < 0)
    // ... erro ...
    return;  // ← aqui pipes já estão em epoll

// Depois em Server.cpp:295-299
if (client->isCgiActive())
{
    int out_fd = client->getCgiOutFd();
    int in_fd = client->getCgiInFd();
    if (out_fd >= 0)
        cgi_fd_map[out_fd] = client;  // ← window de vulnerabilidade
```

**Problema:**
1. `startCgi` adiciona pipes ao epoll (linha 618)
2. Retorna para `Server::handleClientData`
3. `handleClientData` adiciona pipes ao `cgi_fd_map` (linha 150-156)
4. **Window de vulnerabilidade:** Se script escreve output MUITO rápido entre passos 1-3, evento de epoll não encontra entrada em `cgi_fd_map`

**Fluxo Atual:**
```
startCgi() {
    epoll_ctl(ADD, pipe_out[0])  // ← Passo 1: registra no epoll
    return;
}
// ← WINDOW: epoll pode disparar AQUI
handleClientData() {
    processRequest();  // chama startCgi
    cgi_fd_map[pipe_out[0]] = client;  // ← Passo 2: registra no mapa
}
```

**Impacto:** Output pode ser perdido em edge cases com scripts muito rápidos

**Solução Recomendada:**
Registrar pipes em `cgi_fd_map` dentro de `startCgi`, antes de `epoll_ctl`:
```cpp
void Client::startCgi(..., std::map<int, Client*>& cgi_fd_map) {
    // ... fork e exec ...

    // Registrar PRIMEIRO no mapa
    cgi_fd_map[cgi.pipe_out[0]] = this;
    cgi_fd_map[cgi.pipe_in[1]] = this;

    // DEPOIS no epoll
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, cgi.pipe_in[1], &ev);
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, cgi.pipe_out[0], &ev);
}
```

---

## 🟠 FALHAS IMPORTANTES

### 4. **stderr misturado com stdout quebra parsing HTTP**
**Status:** ❌ **AINDA EXISTE** (linhas 641-642)
**Localização:** `src/server/Client.cpp:641-642`

```cpp
dup2(cgi.pipe_out[1], STDOUT_FILENO);
dup2(cgi.pipe_out[1], STDERR_FILENO);  // ← mesmo pipe para ambos
```

**Código Atual:** ✓ Idêntico à documentação

**Problema:**
- Script com `sys.stderr.write("Debug\n")` antes de headers HTTP polui resposta
- Headers malformados quebram parsing
- Logs de debug do PHP/Python aparecem no output HTTP

**Exemplo de Script Problemático:**
```python
#!/usr/bin/python3
import sys
sys.stderr.write("WARNING: something happened\n")  # ← vai para output HTTP
print("Content-Type: text/html\r\n")
print("Hello World")
```

**Output Real:**
```
WARNING: something happened
Content-Type: text/html

Hello World
```

**Resultado:** `"WARNING: something happened\nContent-Type..."` não é header HTTP válido

**Impacto:** Headers malformados, parsing quebrado, exposição de informações de debug

**Solução Recomendada:**
```cpp
// Opção 1: Redirecionar stderr para /dev/null
int devnull = open("/dev/null", O_WRONLY);
dup2(cgi.pipe_out[1], STDOUT_FILENO);
dup2(devnull, STDERR_FILENO);
close(devnull);

// Opção 2: Log file separado (melhor para debug)
int log_fd = open("/var/log/webserv/cgi_errors.log", O_WRONLY | O_APPEND | O_CREAT, 0644);
dup2(log_fd, STDERR_FILENO);
close(log_fd);
```

---

### 5. **Erro de write no stdin deixa cliente preso**
**Status:** ⚠️ **PARCIALMENTE CORRIGIDO** (linhas 761-765)
**Localização:** `src/server/Client.cpp:761-765`

```cpp
else  // w < 0
{
    std::cerr << "[CLIENT " << fd << "] CGI stdin write error: " << strerror(errno) << std::endl;
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, cgi.pipe_in[1], NULL);
    close(cgi.pipe_in[1]);
    cgi.pipe_in[1] = -1;
    // ← AINDA DEIXA CLIENTE PRESO (não finaliza CGI nem gera erro 502)
}
```

**Melhorias em relação ao documento:**
- ✅ Mensagem de erro incluída com strerror(errno)
- ❌ Ainda não chama `finishCgiAndGenerateResponse()` ou mata o processo filho

**Problema:**
- Se pipe quebra (processo filho morreu inesperadamente), apenas fecha pipe
- Não finaliza CGI nem envia resposta de erro
- Cliente fica em estado `CGI_RUNNING` indefinidamente
- Timeout geral (120s) eventualmente fecha, mas resposta lenta

**Cenário:**
1. POST com body grande (10MB)
2. Script CGI crasha após ler 1MB
3. Pipe stdin quebra (EPIPE)
4. `write()` retorna -1 com errno=EPIPE
5. Código fecha pipe mas não finaliza CGI
6. Cliente espera 120s até timeout de inatividade

**Impacto:** Usuário espera timeout completo ao invés de erro imediato

**Solução Recomendada:**
```cpp
else  // w < 0
{
    std::cerr << "[CLIENT " << fd << "] CGI stdin write error: "
              << strerror(errno) << std::endl;
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, cgi.pipe_in[1], NULL);
    close(cgi.pipe_in[1]);
    cgi.pipe_in[1] = -1;

    // ADICIONAR: Finalizar CGI com erro
    if (cgi.pid > 0) {
        kill(cgi.pid, SIGKILL);
        waitpid(cgi.pid, NULL, 0);
    }

    // Gerar resposta de erro 502
    std::string error_msg;
    StatusCodes::http502BadGateway(error_msg, request,
                                    "CGI process terminated unexpectedly",
                                    server_config);
    send_buffer = error_msg;
    send_offset = 0;
    state = SENDING_RESPONSE;
    is_cgi_active = false;
}
```

---

### 6. **Timeout de CGI muito curto e não configurável**
**Status:** ⚠️ **PARCIALMENTE CORRIGIDO** (linha 401)
**Localização:** `src/server/Server.cpp:401`

```cpp
if (now - client->getCgiStartTime() > 10)  // ← hardcoded 10s
```

**Melhorias em relação ao documento:**
- ✅ Timeout existe (não é 0 ou inexistente)
- ✅ Response 504 é enviada antes de fechar (linha 441: `client->sendTimeoutResponse()`)
- ❌ Timeout ainda é hardcoded em 10s
- ❌ Não é configurável por location ou globalmente

**Problema:**
- 10 segundos é muito curto para scripts legítimos
- Scripts que processam imagens, vídeos, ou consultas pesadas são mortos prematuramente
- Não é configurável por location ou globalmente

**Cenários Legítimos Afetados:**
- Processamento de upload de imagem (resize, thumbnails)
- Relatórios complexos com queries SQL
- Conversão de vídeo/áudio
- APIs externas lentas (pagamento, geolocalização)

**Impacto:** Scripts legítimos são interrompidos incorretamente

**Solução Recomendada:**
1. Aumentar padrão para 30s
2. Adicionar diretiva de configuração:
```conf
location /cgi-bin {
    cgi_timeout 60;  # 60 segundos
}
```

3. Implementar timeout diferenciado:
```cpp
// Em LocationConfig
size_t cgi_timeout;  // padrão: 30s

// Em ConfigHelper
if (key == "cgi_timeout")
    location.cgi_timeout = stringToULong(value);

// Em Server::checkTimeout
size_t timeout = location.cgi_timeout > 0 ? location.cgi_timeout : 30;
if (now - client->getCgiStartTime() > timeout)
```

---

## 🟡 FALHAS MÉDIAS

### 7. **Não valida existência do interpretador**
**Status:** ❌ **AINDA EXISTE** (linhas 548-550)
**Localização:** `src/server/Client.cpp:548-550`

```cpp
if (!fileExists(script_path))  // ✓ script validado
    return 404;

std::string interpreter = it->second;  // ✗ interpretador NÃO validado
// ... segue direto para fork() sem validar interpretador ...
```

**Problema:**
- `/usr/bin/python3` inexistente só é descoberto após `fork` → `execve` falha silenciosamente
- Resposta incorreta: 200 OK com body vazio (por causa de falha #2)
- Desperdício de recursos (fork desnecessário)

**Impacto:** Diagnóstico falho, fork desnecessário

**Solução Recomendada:**
```cpp
std::string interpreter = it->second;

// ADICIONAR: Validar interpretador antes de fork
if (!fileExists(interpreter)) {
    std::cerr << "[CGI] Interpreter not found: " << interpreter << std::endl;
    StatusCodes::http502BadGateway(error_msg, req,
                                    "CGI interpreter not found: " + interpreter,
                                    server_config);
    send_buffer = error_msg;
    send_offset = 0;
    state = SENDING_RESPONSE;
    return;
}

// Verificar se é executável
if (access(interpreter.c_str(), X_OK) != 0) {
    std::cerr << "[CGI] Interpreter not executable: " << interpreter << std::endl;
    StatusCodes::http502BadGateway(error_msg, req,
                                    "CGI interpreter not executable",
                                    server_config);
    send_buffer = error_msg;
    send_offset = 0;
    state = SENDING_RESPONSE;
    return;
}
```

---

### 8. **Parsing de headers CGI frágil**
**Status:** ⚠️ **AINDA EXISTE** (linhas 845-847)
**Localização:** `src/server/Client.cpp:845-847`

```cpp
size_t pos = cgi.output.find("\r\n\r\n");
size_t header_end_len = 4;
if (pos == std::string::npos) {
    pos = cgi.output.find("\n\n");
    header_end_len = 2;
}
```

**Melhorias em relação ao documento:**
- ✅ Usa variable `header_end_len` para tracking correto
- ⚠️ Ainda não valida formato de headers antes de usar
- ⚠️ Ainda não detecta headers malformados

**Problema:**
- Não valida formato de headers antes de usar
- Headers sem `\r\n\r\n` nem `\n\n` são tratados como body inteiro
- Parsing não detecta headers malformados
- Não verifica se headers contêm apenas ASCII imprimível

**Cenário:**
Script que esquece linha em branco entre headers e body:
```python
print("Content-Type: text/html")  # ← FALTOU \r\n extra
print("Hello World")
```

**Resultado Atual:**
```
HTTP/1.1 200 OK
Content-Type: text/html
Hello World
Content-Length: 0

```
Body vazio porque tudo foi tratado como headers

**Impacto:** Respostas vazias ou malformadas

**Solução Recomendada:**
```cpp
// Validar headers CGI antes de usar
bool validateCgiHeaders(const std::string& headers) {
    std::istringstream iss(headers);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.empty() && line[line.size()-1] == '\r')
            line.erase(line.size()-1);

        if (line.empty())
            continue;

        // Header deve ter formato "Key: Value"
        size_t colon = line.find(':');
        if (colon == std::string::npos || colon == 0)
            return false;

        // Validar que não há caracteres de controle
        for (size_t i = 0; i < line.size(); i++) {
            unsigned char c = line[i];
            if (c < 32 && c != '\t')  // controle exceto TAB
                return false;
        }
    }
    return true;
}
```

---

### 9. **Sem detecção de sinais do filho**
**Status:** ❌ **AINDA EXISTE** (linhas 800-810)
**Localização:** `src/server/Client.cpp:800-810`

```cpp
int status;
pid_t result = waitpid(cgi.pid, &status, WNOHANG);
if (result == cgi.pid) {
    // status existe mas NÃO é verificado com WIFEXITED, WIFSIGNALED, WTERMSIG
    // apenas drena output e finaliza
}
```

**Problema:**
- Não distingue exit normal de crash (SIGSEGV, SIGILL, SIGABRT)
- Segmentation fault aparece como sucesso
- Diagnóstico impossível para desenvolvedores

**Exemplo:**
```c
// script.cgi
int main() {
    int *ptr = NULL;
    *ptr = 42;  // SIGSEGV
    return 0;
}
```

**Resposta Atual:** 200 OK com body vazio

**Impacto:** Crashes não são reportados, diagnóstico impossível

**Solução Recomendada:**
```cpp
pid_t result = waitpid(cgi.pid, &status, WNOHANG);
if (result == cgi.pid) {
    std::string error_info;

    if (WIFEXITED(status)) {
        int exit_code = WEXITSTATUS(status);
        if (exit_code != 0) {
            std::cerr << "[CGI] Process exited with code " << exit_code << std::endl;
            if (exit_code == 127)
                error_info = "execve failed - interpreter not found";
            else
                error_info = "script exited with non-zero status";
        }
    }
    else if (WIFSIGNALED(status)) {
        int signal = WTERMSIG(status);
        const char* signame = strsignal(signal);
        std::cerr << "[CGI] Process killed by signal " << signal
                  << " (" << signame << ")" << std::endl;
        error_info = std::string("script crashed: ") + signame;
    }

    // Se houve erro, gerar 502
    if (!error_info.empty() && cgi.output.empty()) {
        StatusCodes::http502BadGateway(send_buffer, request, error_info, server_config);
        state = SENDING_RESPONSE;
        is_cgi_active = false;
        return;
    }
}
```

---

## 🔵 FALHAS BAIXAS

### 10. **Configuração dependente de ordem**
**Status:** ✅ **PROBLEMA NÃO ENCONTRADO** (Sintaxe unificada usada)

**Localização:** ConfigParser usa sintaxe unificada

**Problema:**
Parser requer `cgi_extension` antes de `cgi_path` - ordem invertida quebra silenciosamente:

```conf
# ✓ Funciona
cgi_extension .py;
cgi_path /usr/bin/python3;

# ✗ Quebra silenciosamente
cgi_path /usr/bin/python3;
cgi_extension .py;
```

**Impacto:** Configuração não intuitiva, debug difícil

**Solução Recomendada:**
Sintaxe unificada sem dependência de estado:
```conf
cgi_handler .py /usr/bin/python3;
cgi_handler .php /usr/bin/php-cgi;
```

---

### 11. **Responsabilidade duplicada**
**Status:** ✅ **PROBLEMA NÃO ENCONTRADO** (Guards removidos)

**Localização:** Response.cpp (guards em methodGet/methodPost)

**Problema:**
- `Response::methodGet/methodPost` têm guards que retornam 502 se location tem cgi_handlers
- Design redundante com lógica de `Client::processRequest`
- Dois pontos de decisão para mesmo comportamento

**Impacto:** Manutenção duplicada, risco de inconsistência

**Solução Recomendada:**
Remover guards de `Response.cpp` e garantir que `Client::processRequest` sempre captura CGI locations antes de chamar `Response`.

---

## ✅ PROBLEMAS RESOLVIDOS (Confirmar antes de remover de backlog)

### Falha #10: Configuração dependente de ordem
**Status:** ✅ RESOLVIDO - Sintaxe de configuração unificada

Não encontrado no código atual. ConfigParser usa estrutura unificada onde a ordem de `cgi_extension` e `cgi_path` não importa.

---

### Falha #11: Responsabilidade duplicada  
**Status:** ✅ RESOLVIDO - Guards removidos de Response.cpp

Não encontrado no código atual. Response.cpp não contém guards duplicados para verificar cgi_handlers. A lógica está concentrada em Client::processRequest().

---

## 📝 ALTERAÇÕES REALIZADAS (22/03/2026)

Este arquivo foi atualizado com status atual do código. Mudanças:

1. ✅ Adicionado resumo de status em tabela (antes de FALHAS CRÍTICAS)
2. ✅ Marcado com "Status: ❌ AINDA EXISTE" / "⚠️ PARCIALMENTE CORRIGIDO" / "✅ RESOLVIDO"
3. ✅ Atualizado "RECOMENDAÇÕES DE PRIORIZAÇÃO" com localizações atuais no código
4. ✅ Adicionado contexto de código real do projeto (linhas exatas)
5. ✅ Confirmação de que Falhas #10 e #11 foram resolvidas

**Próximas ações:** Selecionar uma falha da lista URGENTE para implementar fixação.

A implementação CGI também possui pontos fortes:

1. **Query string É parseada corretamente** (`HttpRequest.cpp:70-80`)
2. **Timeout de CGI ESTÁ implementado** (contrário à documentação antiga)
3. **Pipes são non-blocking** - evita travamento do servidor
4. **Integração epoll é sólida** - event-driven sem threads
5. **Suporta keep-alive** - múltiplas requisições incluindo CGI
6. **EnvBuilder completo** - variáveis CGI/1.1 corretas

---

## 📋 RECOMENDAÇÕES DE PRIORIZAÇÃO (22/03/2026)

### 🔴 URGENTE (Segurança - 3-5 falhas críticas)
1. **Adicionar limite de memória para `cgi.output`** 
   - Localização: `Client.cpp:774, 805` (handleCgiStdoutReadable)
   - Ação: Adicionar check `if (cgi.output.size() > MAX_CGI_OUTPUT_SIZE)` antes de append
   - Exemplo: `const size_t MAX_CGI_OUTPUT_SIZE = 10 * 1024 * 1024;  // 10MB`

2. **Verificar status de saída do filho com WIFEXITED/WIFSIGNALED**
   - Localização: `Client.cpp:800-810` (finishCgiAndGenerateResponse)
   - Ação: Usar macros WIFEXITED, WIFSIGNALED, WTERMSIG ao processar status
   - Impacto: Diferenciar crashes de exits normais e retornar 502 apropriadamente

### 🟠 IMPORTANTE (Robustez - 3-4 falhas altas)
3. **Remover race condition ao registrar pipes em cgi_fd_map**
   - Localização: `Server.cpp:297-299` (após processRequest retorna)
   - Ação: Mover registro de `cgi_fd_map` para DENTRO de `Client::startCgi()` ANTES de `epoll_ctl`
   - Benefício: Garante que pipes estão em mapa ANTES de serem registrados em epoll

4. **Separar stderr para `/dev/null` ou arquivo de log**
   - Localização: `Client.cpp:641-642` (fork child - dup2 calls)
   - Ação: Mudar STDERR_FILENO de `cgi.pipe_out[1]` para `/dev/null` ou log file
   - Impacto: Evita poluição de headers HTTP com mensagens de debug

5. **Completar handling de erro em write de stdin**
   - Localização: `Client.cpp:761-765` (handleCgiStdinWritable else clause)
   - Ação: Chamar `finishCgiAndGenerateResponse()` com erro 502 ao invés de apenas fechar pipe
   - Impacto: Cliente recebe erro imediato ao invés de esperar timeout

6. **Validar existência do interpretador ANTES de fork**
   - Localização: `Client.cpp:548-550` (startCgi)
   - Ação: Adicionar `fileExists(interpreter)` e `access(interpreter, X_OK)` check
   - Benefício: Evita fork desnecessário e erro 502 imediato

### 🟡 MÉDIO (Usabilidade - 2-3 falhas médias)
7. **Tornar timeout de CGI configurável**
   - Localização: `Server.cpp:401` (checkTimeout)
   - Ação: Adicionar suporte para `cgi_timeout` em LocationConfig
   - Valor padrão: aumentar de 10s para 30s
   - Exemplo: `location /cgi-bin { cgi_timeout 60; }`

8. **Melhorar validação de headers CGI**
   - Localização: `Client.cpp:845-847` (finishCgiAndGenerateResponse)
   - Ação: Adicionar `validateCgiHeaders()` antes de processar
   - Validação: Garantir que headers contêm apenas ASCII válido

9. **Adicionar detecção de sinais do processo filho**
   - Localização: `Client.cpp:800` (finishCgiAndGenerateResponse)
   - Ação: Usar `WIFSIGNALED(status)` e `WTERMSIG(status)` para diagnosticar crashes
   - Benefit: Logs melhores com causa real do crash (SIGSEGV, SIGABRT, etc)

---

## ✅ PROBLEMAS RESOLVIDOS (Confirmar antes de remover de backlog)

---

## 🛡️ CHECKLIST DE SEGURANÇA PARA PRODUÇÃO

Antes de usar em produção, corrigir:

- [ ] Limite de memória para `cgi.output`
- [ ] Validação de status de saída do processo
- [ ] Timeout configurável (mínimo 30s)
- [ ] Separação de stderr
- [ ] Validação de interpretador antes de fork
- [ ] Handling de erro em write de stdin
- [ ] Race condition no registro de pipes

---

## 📚 REFERÊNCIAS

- **CGI/1.1 RFC 3875:** https://www.rfc-editor.org/rfc/rfc3875
- **Documentação interna:** `doc/CGI_IMPLEMENTACAO.md`
- **Código relevante:**
  - `src/server/Client.cpp:474-787` - Ciclo de vida CGI
  - `src/server/Server.cpp:310-367` - Timeout e cleanup
  - `src/cgi/EnvBuilder.cpp` - Variáveis de ambiente
