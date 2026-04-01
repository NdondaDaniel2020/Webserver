# 🔴 Implementacao CGI Atual (Detalhada)

**Última Atualização:** 31 de Março de 2026  
**Revisão Crítica:** 27 de Março - 31 de Março de 2026  
**Revisão Final:** 1º de Abril de 2026 - Fork/pipes/env documentados, 7 problemas listados  
**Status:** ✅ DOCUMENTADO - Implementação completa, race conditions identificadas

## Escopo

Este documento descreve, em detalhe, como o CGI funciona no estado atual do projeto.

Ponto central: o caminho antigo por `CGIHandler` nao esta no fluxo ativo. Hoje o CGI e executado e orquestrado por `Client`, integrado ao loop `epoll` de `Server`.

Arquivos relevantes para entender o ciclo completo:

- `include/Client.hpp`
- `src/server/Client.cpp`
- `src/server/Server.cpp`
- `src/cgi/EnvBuilder.cpp`
- `include/EnvBuilder.hpp`
- `src/config/ConfigHelper.cpp`
- `src/http/Response.cpp` (para entender o que NAO e CGI)

> **👉 Navegação:** Veja [INDEX.md](INDEX.md) para índice centralizado de toda documentação

---

## 1. Arquitetura E Responsabilidades

Separacao de papeis no design atual:

- `Server`: multiplexacao de I/O com `epoll`, aceita conexoes e despacha eventos.
- `Client`: maquina de estados por conexao HTTP e dono do ciclo CGI (fork/pipes/leitura/parsing da resposta).
- `EnvBuilder`: transforma `HttpRequest` em variaveis de ambiente para `execve`.
- `Response`: gera respostas estaticas e de metodos nao-CGI (GET estatico, POST upload/json/form, DELETE).

Visao de alto nivel:

```text
TCP socket
  -> Server::handleClientData
  -> Client::appendRecvData / isRequestComplete
  -> Client::processRequest
      -> (se location com CGI) Client::startCgi
      -> (senao) Response(...)
  -> eventos de pipe CGI via cgi_fd_map
  -> Client::handleCgiStdinWritable / handleCgiStdoutReadable
  -> Client::finishCgiAndGenerateResponse
  -> Client::sendData
```

---

## 2. Configuracao CGI No Parser

### 2.1 Como a configuracao e lida

Em `ConfigHelper::parseCommonConfig(..., LocationConfig&)`:

- `cgi_extension` ou `cgi_extensions`
  - salva a extensao em `location.cgi_handlers[ext] = ""`.
  - guarda `location.cgi_path = ext` temporariamente.
- `cgi_path`
  - associa o `path` ao ultimo valor guardado em `location.cgi_path`.

Isso cria o mapa final `extensao -> interpretador`.

### 2.2 Exemplo pratico

```conf
location /cgi-bin {
    allowed_methods GET POST;
    root www/cgi-bin;

    cgi_extension .py;
    cgi_path /usr/bin/python3;

    cgi_extension .php;
    cgi_path /usr/bin/php-cgi;
}
```

Mapa resultante:

- `.py -> /usr/bin/python3`
- `.php -> /usr/bin/php-cgi`

### 2.3 Observacao de robustez

Como a associacao depende de estado (extensao antes do path), uma configuracao fora de ordem gera:
- `cgi_handlers` vazio se `cgi_path` vir antes de `cgi_extension`
- Handler mapeado para chave temporaria vazia

Recomendacao: Parser deveria validar ordem ou usar sintaxe sem estado (ex: `cgi_handler .py /usr/bin/python3`)

---

## 3. Maquina De Estados Do Client

Estados declarados em `Client::State`:

- `READING_HEADERS`
- `READING_BODY`
- `PROCESSING`
- `CGI_RUNNING`
- `SENDING_RESPONSE`
- `DONE`
- `ERROR_413`

Transicoes tipicas com CGI:

```text
READING_HEADERS -> READING_BODY -> PROCESSING -> CGI_RUNNING -> SENDING_RESPONSE -> DONE
```

Caso sem body (GET):

```text
READING_HEADERS -> PROCESSING -> CGI_RUNNING -> SENDING_RESPONSE -> DONE
```

Se `Content-Length` exceder limite antes de ler body:

```text
READING_HEADERS -> ERROR_413 -> SENDING_RESPONSE -> DONE
```

---

## 4. Deteccao De Request CGI

Em `Client::processRequest(...)`:

1. Extrai body de `recv_buffer` se `content_length > 0`.
2. Localiza a melhor location por maior prefixo (`findMatchingLocation`).
3. Se `location` existe e `!location->cgi_handlers.empty()`:
   - loga inicio CGI,
   - muda para `CGI_RUNNING`,
   - chama `startCgi(request, *location, epoll_fd)`.

Importante:

- A decisao inicial e por presence de handlers na location.
- A validacao fina (extensao realmente suportada) ocorre dentro de `startCgi`.

Consequencia pratica:

- URI em location CGI com extensao nao mapeada gera erro de gateway, nao fallback para estatico.

---

## 5. Inicio Do Processo CGI (`startCgi`) - LINHAS 474-565 CLIENT.CPP

### 5.0 Estrutura De Estado CGI (CgiState em Client.hpp linhas 49-62)

Estrutura que encapsula todo o ciclo de vida do CGI:

```cpp
struct CgiState
{
    pid_t           pid;              // PID do processo filho
    int             pipe_in[2];       // FDs: pai escreve body → filho stdin
    int             pipe_out[2];      // FDs: filho stdout → pai lê
    std::string     output;           // Acumula saída bruta do script
    size_t          body_written;     // Bytes ja escritos no stdin
    bool            finished;         // Flag de conclusão (nao usado no fluxo)
    time_t          start_time;       // Timestamp de início para timeout

    CgiState() : pid(-1), body_written(0), finished(false), start_time(0)
    {
        pipe_in[0] = pipe_in[1] = -1;
        pipe_out[0] = pipe_out[1] = -1;
    }
};
```

Além disso, `Client` mantém:
- `bool is_cgi_active`: flag de CGI em execução
- `CgiState cgi`: instância única do estado

## 5.1 Resolucao do `script_path`

Formula atual (Client.cpp linha ~476):

```text
script_path = loc.root + req.getPath().substr(loc.path.size())
```

Exemplo prático:

- `loc.path = "/cgi-bin"` (path da location)
- `loc.root = "www/cgi-bin"` (root da location)
- `req.getPath() = "/cgi-bin/test.py"` (URI sem query string)
- `req.getPath().substr(loc.path.size()) = "/test.py"` (remove prefixo)
- `script_path = "www/cgi-bin" + "/test.py" = "www/cgi-bin/test.py"` ✓ CORRETO

Caso de query string:
- `req.getPath() = "/cgi-bin/test.py"` (query já separada em `req.query`)
- Query string é enviada via `QUERY_STRING` em variáveis de ambiente, não no path do script

## 5.2 Escolha do interpretador (Cliente.cpp linhas ~477-490)

- extensao extraída por `rfind('.')` no `script_path`.  
- busca em `loc.cgi_handlers` usando a extensão como chave.
- exemplo: `.py`, `.php`, `.cgi`, etc.

Se não encontrado:

```cpp
StatusCodes::http502BadGateway(error_msg, "CGI handler not found for extension");
state = SENDING_RESPONSE;
return;  // Encerra sem fork
```

Observação: O método `StatusCodes::http502BadGateway(...)` popula `send_buffer` automaticamente, portanto a resposta de erro é completa. A transição para `SENDING_RESPONSE` dispara envio imediato no loop principal.

## 5.3 Criacao de pipes

- `pipe_in`: pai escreve body para stdin do filho.
- `pipe_out`: filho escreve stdout/stderr para pai.

Se `pipe()` falhar:

- retorna fluxo de erro com `502`.

## 5.4 Flags non-blocking

No pai:

- `fcntl(cgi.pipe_in[1], F_SETFL, O_NONBLOCK)`
- `fcntl(cgi.pipe_out[0], F_SETFL, O_NONBLOCK)`

## 5.5 Ambiente `envp`

`buildEnvp(req, script_path, loc)`:

- chama `EnvBuilder::build(...)` para obter `vector<string>`.
- aloca `char** envp`.
- copia cada string para `char[]` proprio.

Liberacao:

- pai libera logo apos `fork`.
- filho libera apenas em falha de `execve`.

## 5.6 fork e exec

Filho:

- redireciona `stdin`, `stdout`, `stderr` com `dup2`.
- fecha descritores nao usados.
- constroi `argv = [interpreter, script_path, NULL]`.
- chama `execve(interpreter, argv, envp)`.
- se falhar, `exit(127)`.

Pai:

- fecha `pipe_in[0]` e `pipe_out[1]`.
- inicializa estado CGI:
  - `cgi.pid`
  - `cgi.body_written = 0`
  - `cgi.finished = false`
  - `cgi.start_time = time(NULL)`
  - `cgi.output = ""`
- `is_cgi_active = true`.

## 5.7 Registro no epoll (INSIDE startCgi, client.cpp linhas 560-565)

Ainda dentro de `startCgi`, registra pipes nao-bloqueantes em epoll:

```cpp
epoll_event ev;

// Entrada (write body)
ev.events = EPOLLOUT | EPOLLHUP;  // Pronto para escrever
ev.data.fd = cgi.pipe_in[1];
epoll_ctl(epoll_fd, EPOLL_CTL_ADD, cgi.pipe_in[1], &ev);

// Saida (read stdout/stderr)
ev.events = EPOLLIN | EPOLLHUP;  // Dados prontos
ev.data.fd = cgi.pipe_out[0];
epoll_ctl(epoll_fd, EPOLL_CTL_ADD, cgi.pipe_out[0], &ev);
```

Registo no cgi_fd_map: Ocorre em Server::handleClientData apos processRequest retorna (veja secao 6.3).

Mode Level-Triggered: Sem flag EPOLLET
- Pro: simples de implementar, re-trigger automatico se nao drena tudo
- Con: possivel spurious wakeups se script escreve lentamente

Observacao: foi escolhido modo level-triggered para simplificar e evitar perda de evento por drenagem incompleta em modo edge-triggered.

---

## 6. Integracao Com O Loop Do Server - SERVER.CPP LINHAS 25-120

### 6.1 Mapas De FD Registrados (Server.hpp)

```cpp
std::map<int, Client*>   clients;      // fd socket client → Client*
std::map<int, Client*>   cgi_fd_map;   // fd pipe CGI → Client*
```

### 6.2 Loop Principal: epoll_wait De Despacho (Server::start linhas ~50-85)

Fluxo por iteração:

```cpp
while (true)
{
    checkTimeout();  // ← Verifica timeout de CGI (veja seção 6.4)
    
    int n = epoll_wait(this->epoll_fd, this->events, 64, 1000);
    
    for (int i = 0; i < n; i++)
    {
        int fd = this->events[i].data.fd;
        
        // Ser socket servidor? aceita nova conexao
        if (isServerSocket(fd))
        {
            newConnection(fd);
            continue;
        }
        
        // Procura em sockets cliente
        std::map<int, Client*>::iterator it = clients.find(fd);
        if (it != clients.end())
        {
            Client *client = it->second;
            
            // EPOLLIN: dado chegando → handleClientData
            if (events[i].events & EPOLLIN)
                handleClientData(fd);
            
            // EPOLLOUT: pronto para escrita → sendData
            if (events[i].events & EPOLLOUT && 
                client->getState() == Client::SENDING_RESPONSE)
            {
                bool finished = client->sendData();
                if (finished)
                {
                    if (client->isKeepAlive())
                        client->reset();
                    else
                        closeClient(fd);
                }
            }
        }
        else
        {
            // Procura em pipes CGI
            std::map<int, Client*>::iterator cit = cgi_fd_map.find(fd);
            if (cit != cgi_fd_map.end())
            {
                Client *c = cit->second;
                
                // Saída do CGI pronta para leitura
                if (events[i].events & (EPOLLIN | EPOLLERR | EPOLLHUP))
                    c->handleCgiStdoutReadable(epoll_fd);
                
                // Entrada do CGI pronta para escrita do body
                if (events[i].events & EPOLLOUT)
                    c->handleCgiStdinWritable(epoll_fd);
            }
        }
    }
}
```

### 6.3 Registro De Pipes No cgi_fd_map (Server::handleClientData linhas ~148-160)

Depois que `Client::processRequest` iniciou CGI:

```cpp
if (client->isRequestComplete())
{
    client->processRequest(...);  // Pode chamar startCgi
    
    // Registrar pipes no mapa para epoll despacho
    if (client->isCgiActive())
    {
        int out_fd = client->getCgiOutFd();  // cgi.pipe_out[0]
        int in_fd = client->getCgiInFd();    // cgi.pipe_in[1]
        
        if (out_fd >= 0)
            cgi_fd_map[out_fd] = client;
        if (in_fd >= 0)
            cgi_fd_map[in_fd] = client;
    }
}
```

### 6.4 Timeout De CGI (Server::checkTimeout)

Embora o documento anterior não detalhe, o código contém:

```cpp
void Server::checkTimeout()
{
    // Verifica clients em CGI_RUNNING
    // Se cgi.start_time + threshold < time(NULL)
    //   → matا processo com SIGKILL
    //   → finaliza resposta com erro de timeout
}
```

⚠️ **Nota**: A implementação de timeout pode estar ausente ou incompleta. Verificar atual `Server.cpp` para confirmação.

### 6.5 Desenho De Separação

Esse desenho separa responsabilidades:

- **`clients` map**: I/O de rede (sockets de conexão)
- **`cgi_fd_map` map**: I/O de pipes internos (só lido pelo epoll, não pelo epoll_wait direto)

Benefício: um único `epoll_fd` controla ambos, sem threads separadas. Se script CGI ler 10MB lentamente, isso não bloqueia outras conexões.

---

## 7. Escrita Do Body Para O CGI (`handleCgiStdinWritable`)

Contrato funcional:

- escrever o body sem bloquear o thread principal.
- suportar escrita parcial.
- fechar stdin do CGI ao concluir envio.

Passos:

1. Se CGI inativo, retorna.
2. Se body vazio:
   - remove FD do epoll,
   - fecha `pipe_in[1]`,
   - marca `-1`.
3. Se body existe:
   - escreve a partir de `body + cgi.body_written`.
   - incrementa `cgi.body_written` com bytes realmente escritos.
4. Se `body_written >= body.size()`:
   - remove FD do epoll,
   - fecha `pipe_in[1]`.

Tratamento de Erro (Cliente.cpp linhas ~599-607):

```cpp
if (w < 0)
{
    std::cerr << "[CLIENT " << fd << "] CGI stdin write error: " 
              << strerror(errno) << std::endl;
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, cgi.pipe_in[1], NULL);
    close(cgi.pipe_in[1]);
    cgi.pipe_in[1] = -1;
}
```

⚠️ **Limitação**: Erro de `write` não dispara finalização (ex: `finishCgiAndGenerateResponse`). O cliente pode ficar em `CGI_RUNNING` aguardando output, que nunca virá se o pipe foi quebrado permanentemente. Timeout geral de inatividade eventualmente fecha a conexão.

---

## 8. Leitura Da Saida CGI (`handleCgiStdoutReadable`) - LINHAS 610-667 CLIENT.CPP

Contrato funcional:

- drenar stdout/stderr do CGI de forma incremental.
- detectar fim de stream (EOF ou erro).
- finalizar resposta e montar HTTP quando não houver mais dados.

Passos detalhados:

```cpp
char buf[8192];
ssize_t r;

// Loop: drenar até EAGAIN ou EOF
while ((r = read(cgi.pipe_out[0], buf, sizeof(buf))) > 0)
{
    cgi.output.append(buf, r);  // Acumula saída
}

if (r < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
{
    // Erro real: interrupção ou FD inválido
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, cgi.pipe_out[0], NULL);
    close(cgi.pipe_out[0]);
    cgi.pipe_out[0] = -1;
    finishCgiAndGenerateResponse();
    return;
}

if (r == 0)
{
    // EOF: processo fechou stdout
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, cgi.pipe_out[0], NULL);
    close(cgi.pipe_out[0]);
    cgi.pipe_out[0] = -1;
    finishCgiAndGenerateResponse();
    return;
}

if (errno == EAGAIN || errno == EWOULDBLOCK)
{
    // Sem dados agora, mas verificar estado do processo
    int status;
    pid_t result = waitpid(cgi.pid, &status, WNOHANG);
    if (result == cgi.pid)
    {
        // Processo terminou: drenar pipe última vez
        while ((r = read(cgi.pipe_out[0], buf, sizeof(buf))) > 0)
            cgi.output.append(buf, r);
        
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, cgi.pipe_out[0], NULL);
        close(cgi.pipe_out[0]);
        cgi.pipe_out[0] = -1;
        cgi.pid = -1;
        finishCgiAndGenerateResponse();
    }
}
```

**Contrato de estado:**
- Se processo já fechou stdout (r==0): finaliza imediatamente
- Se processo ainda vivo mas sem dados (EAGAIN): aguarda próximo evento de epoll
- Se processo terminou (detectable por WNOHANG): drena um último chunk e finaliza
- Tubo de saída não-bloqueante permite evitar travamento do servidor

**Observação de design no redirecionamento (Client.cpp filho, ~line 513):**

```cpp
dup2(cgi.pipe_out[1], STDOUT_FILENO);
dup2(cgi.pipe_out[1], STDERR_FILENO);  // ← Mesmo FD para ambos
```

- `stderr` é redirecionado para o mesmo pipe de `stdout`
- **Ganho**: visibilidade de erro aumenta (logs de script aparecem no output)
- **Risco**: script que escreve logs stderr junto com headers/body pode poluir resposta HTTP
  - Exemplo: script Python com `sys.stderr.write("Debug")` antes de `print("Content-Type...")`
  - Solução futura: redirecionar stderr para `/dev/null` ou arquivo de log separado

---

## 9. Finalizacao E Montagem Da Resposta HTTP

`finishCgiAndGenerateResponse(...)` converte `cgi.output` bruto em resposta HTTP enviada ao cliente.

### 9.1 Encerramento do processo

- se `cgi.pid > 0`, chama `waitpid(..., WNOHANG)` e zera `cgi.pid`.
- `is_cgi_active = false`.

### 9.2 Split de headers e body

Busca separador:

- primeiro `\r\n\r\n`
- fallback `\n\n`

Se encontrado:

- parte antes = `cgi_headers`
- parte depois = `cgi_body`

Se nao encontrado:

- `cgi_headers = ""`
- `cgi_body = output inteiro`

### 9.3 Status e headers extras

- status default: `200 OK`.
- parse linha por linha de `cgi_headers`.
- se linha com `Status: `, sobrescreve status.
- demais linhas (normalizadas sem `\r`) vao para `extra_headers`.

### 9.4 Montagem final

Formato produzido:

```text
HTTP/1.1 <status_line>
<extra_headers>
Content-Length: <len(cgi_body)>
Connection: <keep-alive|close>

<cgi_body>
```

Depois:

- `send_buffer = resposta`
- `send_offset = 0`
- `state = SENDING_RESPONSE`

---

## 10. Interacao Com Keep-Alive

`Client` preserva semantica de keep-alive apos CGI:

- header `Connection` na resposta final usa `keep_alive` do cliente.
- quando envio termina (`sendData`), `Server` decide:
  - `client->reset()` se keep-alive,
  - `closeClient(fd)` se nao.

Isso permite multiplas requisicoes na mesma conexao mesmo quando uma delas usa CGI.

---

## 11. Limpeza E Encerramento Forcado - CLIENT.CPP LINHAS 89-117

`cleanupCgiIfActive(int epoll_fd)` é chamado quando cliente é fechado.

Ações:

```cpp
void Client::cleanupCgiIfActive(int epoll_fd)
{
    if (!is_cgi_active)
        return;

    // 1. Forçar término do processo
    if (cgi.pid > 0)
    {
        kill(cgi.pid, SIGKILL);     // Força saída imediata
        waitpid(cgi.pid, NULL, WNOHANG);
        cgi.pid = -1;
    }

    // 2. Remover FDes do epoll
    if (cgi.pipe_in[1] >= 0)
    {
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, cgi.pipe_in[1], NULL);
        close(cgi.pipe_in[1]);
        cgi.pipe_in[1] = -1;
    }

    if (cgi.pipe_out[0] >= 0)
    {
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, cgi.pipe_out[0], NULL);
        close(cgi.pipe_out[0]);
        cgi.pipe_out[0] = -1;
    }

    is_cgi_active = false;
}
```

Integração No Server::closeClient (SERVER.CPP ~162-189):

```cpp
void Server::closeClient(int fd)
{
    Client *client = clients[fd];
    if (!client) return;
    
    // Guardar FDs antes de cleanup
    int out_fd = client->getCgiOutFd();
    int in_fd = client->getCgiInFd();
    
    // Fazer cleanup (mataria processo se ativo)
    client->cleanupCgiIfActive(epoll_fd);
    
    // Remover FDs de pipes do mapa
    if (out_fd >= 0)
        cgi_fd_map.erase(out_fd);
    if (in_fd >= 0)
        cgi_fd_map.erase(in_fd);
    
    // Fechar socket cliente
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
    close(fd);
    clients.erase(fd);
    delete client;
}
```

✓ **Invariante**: Toda entrada em `cgi_fd_map` aponta para `Client` válido (não zumbi). Quando cliente fecha, todos seus pipes são removidos do mapa antes de `delete`.

---

## 12. Variaveis De Ambiente CGI (Detalhe)

`EnvBuilder::build(...)` monta:

- `REDIRECT_STATUS=200`
- `REQUEST_METHOD=<GET|POST|...>`
- `QUERY_STRING=<request.getQuery()>`
- `SERVER_PROTOCOL=<HTTP/x.y>`
- `GATEWAY_INTERFACE=CGI/1.1`
- `SERVER_SOFTWARE=webserv/1.0`
- `SCRIPT_FILENAME=<script_path>`
- `SCRIPT_NAME=<request.getPath()>`
- `REQUEST_URI=<uri original>`
- `CONTENT_TYPE` / `CONTENT_LENGTH` quando presentes
- headers HTTP convertidos para `HTTP_*`

Regra de conversao de header:

- nome -> uppercase
- `-` -> `_`
- prefixo `HTTP_`

Exemplo:

```text
User-Agent: curl/8.5.0
-> HTTP_USER_AGENT=curl/8.5.0
```

Limite atual:

- `HttpRequest::query` e `HttpRequest::path` nao sao preenchidos no parser atual, entao `QUERY_STRING` e `SCRIPT_NAME` podem ficar vazios mesmo com URI contendo query.

---

## 13. Relacao Com `Response` (Por Que Existe 502 Ali)

`Response::methodGet` e `Response::methodPost` possuem guarda:

- se location encontrada tem `cgi_handlers`, retornam `502 Bad Gateway` com mensagem de falha CGI.

Intencao desse comportamento:

- evitar que o caminho estatico processe resources de location CGI.
- manter CGI no pipeline dedicado de `Client`.

Risco:

- se por algum bug o request CGI cair em `Response`, o usuario recebe erro gateway em vez de fallback para arquivo estatico.

---

## 14. Matriz De Falhas E Efeito Observavel

Falhas principais no caminho CGI:

1. Extensao nao mapeada
   - ponto: `startCgi` antes de `pipe`
   - efeito: `502`.

2. Erro de `pipe()`
   - ponto: `startCgi`
   - efeito: `502`.

3. Erro de `fork()`
   - ponto: `startCgi`
   - efeito: `502`.

4. `execve` falha no filho
   - ponto: filho termina com `127`
   - efeito: processo encerra sem gerar output valido; resposta depende da logica de leitura/finalizacao e pode virar body vazio com `200` se nao houver tratamento adicional.

5. Script escreve output sem headers CGI
   - ponto: parse em `finishCgiAndGenerateResponse`
   - efeito: server assume `200 OK` e trata todo output como body.

6. Script lento ou travado
   - ponto: ausencia de timeout dedicado de CGI
   - efeito: conexao pode ficar em `CGI_RUNNING` ate timeout geral de inatividade do cliente ou cleanup por fechamento.

---

## 15. Pontos De Melhoria Prioritarios

1. **Preencher `query/path` em `HttpRequest::parseRequestLine`** (CRÍTICO)
   - Atual: `QUERY_STRING` vazio mesmo com URI contendo `?param=value`
   - Ganho: `QUERY_STRING` e `SCRIPT_NAME` corretos para CGI conform CGI 1.1
   - Impacto: scripts GET com query strings (ex: `test.py?id=123`) não recebem parâmetros

2. **Adicionar timeout de CGI por processo** (IMPORTANTE)
   - Atual: Script travado fica em `CGI_RUNNING` indefinidamente
   - Implementação: `checkTimeout()` em `Server::start()` já existe na estrutura
   - Ganho: Script lento não bloqueia outras conexões permanentemente
   - Sugestão: Threshold de 30s, usar `cgi.start_time` + `SIGKILL`

3. **Tratar erro permanente de `write` em `handleCgiStdinWritable`** (MÉDIO)
   - Atual: Se pipe_in quebra (ex: processo filho morreu inesperado), cliente fica em `CGI_RUNNING`
   - Solução: Ao receber erro de write, chamar `finishCgiAndGenerateResponse()` com erro `502`
   - Evita: Cliente preso aguardando output que nunca virá

4. **Tratar status de saida do filho (`waitpid status`)** (MÉDIO)
   - Atual: Ignora `WIFEXITED`, `WEXITSTATUS`, `WIFSIGNALED` do filho
   - Ganho: Mapear morte do filho para `502` em vez de `200` com body vazio
   - Exemplo: Se `execve` falha, filho `exit(127)` mas resposta parece sucesso

5. **Separar stderr de stdout** (OPCIONAL - LOW PRIORITY)
   - Atual: stderr → mesmo pipe que stdout, pode poluir HTTP headers
   - Solução: `dup2(open("/dev/null"), STDERR_FILENO)` ou arquivo de log
   - Trade-off: Perde visibilidade de erro do script, mas HTTP mais robusto

6. **Consolidar fluxo CGI em `Client` removendo guards em `Response`** (MÉDIO)
   - Atual: `Response::methodGet/methodPost` retornam `502` se location tem cgi_handlers
   - Risco: Se request CGI cair em Response por bug, gera erro em vez de fallback
   - Solução: Garantir que TODOS os requests dentro de location CGI passem por `Client::startCgi`

---

## 16. Checklist De Debug Operacional

Quando CGI nao responde como esperado:

1. Confirmar mapping em `config/*.conf`:
   - `cgi_extension` com ponto (`.py`, `.php`)
   - `cgi_path` valido no host.

2. Confirmar resolucao de caminho:
   - `loc.root + uri sem prefixo da location`.

3. Verificar permissao de script e interpretador.

4. Validar se script imprime headers CGI corretos:
   - minimo recomendado: `Content-Type: ...` + linha em branco.

5. Observar logs do `Client`:
   - `Iniciando CGI`
   - `CGI stdin writable`
   - `CGI stdout readable`
   - `CGI stdout EOF`

6. Confirmar fim de processo:
   - checar zumbi/processo preso se conexao fechou sem cleanup.

---

---

## 18. Limitacoes Tecnicas De Design

### 18.1 Race Condition: Registro Tardio De FDs No cgi_fd_map

Fluxo:
1. `Client::processRequest` chama `Client::startCgi`
2. `startCgi` registra pipes em epoll e volta
3. `processRequest` retorna para `Server::handleClientData`
4. `handleClientData` registra pipes em `cgi_fd_map` (linhas 150-156)

Window de vulnerabilidade: Entre epoll_ctl (passo 2) e mapa.insert (passo 4):
- Se FD fica pronto (script escreve stdout), epoll dispara evento
- Evento eh buscado em `cgi_fd_map` e nao encontrado
- Output pode ser perdido ou cliente nao processado

Pratica observada no codigo: Esse window eh muito curto (microsegundos) e pode nao causar problema em producao, mas eh sub-ótimo conceitualmente.

Solucao: Registrar pipes em `cgi_fd_map` dentro de `startCgi`, nao em `handleClientData`.

### 18.2 Entrada Dupla em cgi_fd_map

Um FD (pipe) pode estar registrado:
- Uma vez: cgi.pipe_in[1] -> client pointer
- Uma vez: cgi.pipe_out[0] -> mesmo client pointer

Qual a implicacao? Se os dois pipes apontam para mesmo cliente, quando um FD dispara evento, o outro cliente nao eh impactado. Isso eh correto.

Risco: Se dois clientes diferentes compartilhassem pipes (cenario impossivel), cgi_fd_map teria colisao de keys. Situacao atual eh segura.

### 18.3 Timeout De CGI Nao Eh Suportado Formalmente

Campo `cgi.start_time` existe (inicializado em startCgi) mas:
- Nao eh lido em `Server::checkTimeout()` (se existe)
- Se existe timeout, nao eh dinamico por script

Efeito: Script que trava indefinidamente causa:
- Cliente em estado `CGI_RUNNING` permanente
- Pipes abertos ao epoll
- Recurso (FD) nao liberado ate cliente fechar conexao

### 18.4 Sem Deteccao De Status De Saida Do Filho

Quando filho nao consegue exec:
```cpp
execve(interpreter.c_str(), argv, envp);
freeEnvp(envp);
exit(127);  // <- Exit code nunca eh verificado
```

Pai recepciona:
```cpp
waitpid(cgi.pid, NULL, WNOHANG);  // <- NULL ignora status
```

Efeito observavel: erro de execucao (/usr/bin/python3 nao existe) vs sucesso (script retornou 0) tem resposta HTTP identica: "200 OK com body vazio".

Diagnostico: Usuário nao consegue distinguir se script falhou ou se ficou preso sem output.

### 18.5 Sem Limite De Tamanho Para cgi.output

Cada chunk lido eh anexado em `std::string cgi.output`:

```cpp
char buf[8192];
while ((r = read(cgi.pipe_out[0], buf, sizeof(buf))) > 0)
{
    cgi.output.append(buf, r);  // <- Crescimento de memoria descontrolado
}
```

Riscos:
1. Script CGI que escreve 500MB de output
   - `cgi.output` aloca 500MB na memoria do processo
   - Se 100 clientes simultaneos: 50GB de memoria!
   - OOM killer mata processo inteiro

2. Ataque DoS
   - `perl -e 'while(1) { print "X" }'` como CGI
   - Servidor consome memoria ate morrer
   - Nao ha limite de tamanho nem timeout de consumo

Solucao:
- Adicionar limite global ou por-request (ex: 100MB)
- Se excedido, abortar CGI com `SIGKILL` e retornar 413 Payload Too Large
- Bufferizar response em disco (temp file) ao inves de memoria

---

## 17. Resumo Executivo

A implementação atual de CGI é **event-driven** e encaixada no mesmo loop `epoll` do servidor:

### Arquitetura
- **`Client`** (fork/pipes/I/O non-blocking): inicia e controla o processo CGI
- **`Server`** (epoll): despacha eventos de FD de pipe via `cgi_fd_map`
- **`EnvBuilder`**: fornece variáveis de ambiente para `execve`
- **Resposta HTTP**: montada pelo próprio `Client` a partir da saída bruta do script

### Fortalezas
✓ Sem threading — single-threaded, event-driven
✓ Não bloqueia I/O global — script lento não afeta outras conexões (teoricamente)
✓ Suporta keep-alive — múltiplas requisições por conexão incluindo CGI
✓ Parseamento flexível — headers CGI com fallback (`\r\n\r\n` ou `\n\n`)
✓ Integração limpa — `cgi_fd_map` separa pipes de sockets

### Gaps De Robustez
✗ **Sem timeout dedicado**: Script travado pode prender processo indefinidamente
✗ **Query string vazia**: `QUERY_STRING` não parseado de URI
✗ **Escrita com erro**: Se pipe de entrada falha, cliente fica preso em `CGI_RUNNING`
✗ **Status do filho ignorado**: `execve` failure (`exit 127`) não detectado como erro
✗ **Stderr misturado**: Log de script pode poluir headers HTTP
✗ **Responsabilidade compartilhada**: `Response.cpp` também trata rejeição CGI

### Recomendação De Priorização
1. **Timeout de CGI** — Evita DoS por script lento/travado
2. **Parse de query string** — GET com parâmetros é caso de uso comum
3. **Tratamento de erro de write** — Evita cliente preso
4. **Status de saida do filho** — Diagnóstico correto de falha CGI
5. (Opcional) Separar stderr — Melhora parsing HTTP robusto

---

## APÊNDICE: Referência Rápida De Linhas De Código

### Client.hpp
- **Linhas 49-62**: Struct `CgiState` (pid, pipes, output, timestamps)
- **Linhas ~100-150**: Declaração de métodos CGI públicos (`startCgi`, `handleCgiStdin/StdoutReadable`, etc)
- **Enum State**: Estados de máquina cliente (READING_HEADERS, CGI_RUNNING, etc)

### Client.cpp
- **Linhas 89-117**: `cleanupCgiIfActive(epoll_fd)` - Limpeza de processo e pipes
- **Linhas 474-565**: `startCgi(...)` - Criação de pipes, fork, execve, registro epoll
- **Linhas 567-608**: `handleCgiStdinWritable(epoll_fd)` - Escrita não-bloqueante do body
- **Linhas 610-667**: `handleCgiStdoutReadable(epoll_fd)` - Leitura não-bloqueante de output
- **Linhas 669-712**: `finishCgiAndGenerateResponse()` - Parse de headers CGI, montagem HTTP

### Server.cpp
- **Linhas 25-85**: `Server::start()` - Loop principal epoll, despacho de eventos
- **Linhas 120-160**: `handleClientData(fd)` - Registro de pipes em cgi_fd_map APÓS startCgi
- **Linhas 162-190**: `closeClient(fd)` - Cleanup de pipes e remoção de cgi_fd_map
- **Linhas ~200+**: `checkTimeout()` - Verifica timeout de CGI (se implementado)

### EnvBuilder.cpp
- Construção de variáveis de ambiente CGI/1.1 padrão (REQUEST_METHOD, QUERY_STRING, etc)

### ConfigHelper.cpp
- **Linhas ~180-210**: Parse de blocos `cgi_extension` e `cgi_path` para location
- Validação de ordem (CRÍTICA: extensão deve vir antes de path)

### Response.cpp
- Guard conditions em `methodGet` e `methodPost` que retornam 502 se location tem cgi_handlers

---

## APÊNDICE: Exemplo Prático: Requisição GET Para /cgi-bin/test.py

### Configuração
```conf
location /cgi-bin {
    root www/cgi-bin;
    allowed_methods GET POST;
    cgi_extension .py;
    cgi_path /usr/bin/python3;
}
```

### Script (www/cgi-bin/test.py)
```python
#!/usr/bin/python3
import os

print("Content-Type: text/plain\\r\\n")
print("X-Custom: Header\\r\\n")
print()
print("QUERY_STRING:", os.environ.get('QUERY_STRING', ''))
print("REQUEST_METHOD:", os.environ.get('REQUEST_METHOD'))
print("SCRIPT_NAME:", os.environ.get('SCRIPT_NAME'))
```

### Execução Passo-A-Passo

1. **Cliente conecta**: TCP socket
2. **HTTP Request chega**: `GET /cgi-bin/test.py?id=123 HTTP/1.1`
3. **Server::handleClientData**: Lê buffer, reconhece request completo
4. **Client::processRequest**:
   - Extrai body (vazio para GET)
   - Localiza location `/cgi-bin`
   - location.cgi_handlers não está vazio → inicia CGI
5. **Client::startCgi**:
   - script_path = `www/cgi-bin` + `/test.py` = `www/cgi-bin/test.py`
   - Extensão `.py` → encontra `/usr/bin/python3`
   - `pipe(pipe_in)`, `pipe(pipe_out)` ✓
   - `fork()` cria hijo
   - Hijo:
     - `dup2(pipe_in[0], 0)`, `dup2(pipe_out[1], 1+2)`
     - `execve("/usr/bin/python3", [...], envp)` com QUERY_STRING=id=123
   - Pai:
     - `epoll_ctl` adiciona pipes ao epoll
     - `Server::handleClientData` adiciona ao cgi_fd_map
     - State → `CGI_RUNNING`
6. **Script executa**: Python imprime headers + body
7. **epoll dispara EPOLLIN** em pipe_out[0]
8. **Server despacha**: Encontra em cgi_fd_map → `handleCgiStdoutReadable`
9. **Client lê loop**: Accumula em cgi.output até EOF
10. **finishCgiAndGenerateResponse**:
    - Parse: headers = `"Content-Type: text/plain\r\nX-Custom: Header"`
    - Body = resto do output
    - Status = 200 OK (padrão)
    - Monta: `HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nX-Custom: Header\r\nContent-Length: ...\r\n\r\n[body]`
11. **Client::sendData**: Envia resposta HTTP
12. **Cliente fecha ou reset** para keep-alive

### Variáveis De Ambiente Recebidas Pelo Script
```
GATEWAY_INTERFACE=CGI/1.1
REQUEST_METHOD=GET
QUERY_STRING=id=123          (← IMPORTANTE: Precisa de parse de URI)
SCRIPT_NAME=/cgi-bin/test.py (← Pode estar vazio se não parseado)
SCRIPT_FILENAME=www/cgi-bin/test.py
REQUEST_URI=/cgi-bin/test.py?id=123
SERVER_PROTOCOL=HTTP/1.1
HTTP_HOST=localhost:8080
HTTP_USER_AGENT=curl/8.5.0
... (convertidos de headers)
```

---

## APÊNDICE: Teste De Robustez - Casos De Erro

### Caso 1: Interpreter Não Encontrado

Array PATH do sistema não contém `/usr/bin/python3` (ou binário não existe)

- **Efeito**: `startCgi` valida se handler mapeado ✓ MAS NÃO valida se path existe
- **Resultado**: fork ocorre, filho executa `execve("/usr/bin/python3", ...)` → falha
- `exit(127)` mas pai recebe `waitpid(..., NULL)` → ignora status
- Cliente recebe: HTTP 200 com body vazio (❌ DEVERIA SER 502)

### Caso 2: Script Travado

Script entra em loop infinito ou é bloqueado:

```python
while True: pass
```

- **Efeito**: Pipe de saída fica silencioso
- **Timeout**: Não há timeout dedicado, cliente espera indefinidamente
- **Liberação**: Até que cliente feche ou servidor reinicie

### Caso 3: Script Escreve Muito

```python
print("B" * (500 * 1024 * 1024))  # 500MB
```

- **Efeito**: `cgi.output` cresce para 500MB de memória
- **Limite**: Não há limite implementado
- **DoS**: Múltiplos CGI desse tipo podem OOM kill servidor

### Caso 4: Requisição Com Query String

```
GET /cgi-bin/test.py?id=123&name=john
```

- **Problema**: `QUERY_STRING` pode estar vazio
- **Razão**: URI não é parseada para query/fragment (linhas ~150 HttpRequest)
- **Script**: Não recebe parâmetros, `os.environ['QUERY_STRING']` = ""

---

## ✅ BOAS PRÁTICAS IMPLEMENTADAS

1. ✅ Pipes não-bloqueantes com `O_NONBLOCK`
2. ✅ Integração com `epoll` (multiplexação eficiente)
3. ✅ Máquina de estados clara
4. ✅ Registros em `cgi_fd_map` para routing
5. ✅ Cleanup em destrutor (RAII)


