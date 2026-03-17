# Implementacao CGI Atual (Detalhada)

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

Como a associacao depende da ordem (extensao antes do path), uma configuracao fora de ordem pode gerar mapeamento incorreto.

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

## 5. Inicio Do Processo CGI (`startCgi`)

## 5.1 Resolucao do `script_path`

Formula atual:

```text
script_path = loc.root + req.getUri().substr(loc.path.size())
```

Exemplo:

- `loc.path = /cgi-bin`
- `loc.root = www/cgi-bin`
- `uri = /cgi-bin/test.py`
- resultado: `www/cgi-bin/test.py`

## 5.2 Escolha do interpretador

- extensao extraida por `rfind('.')`.
- busca em `loc.cgi_handlers`.

Se nao encontrado:

- `StatusCodes::http502BadGateway(error_msg, "CGI")`
- `state = SENDING_RESPONSE`
- encerra sem fork.

Observacao: o codigo gera `error_msg` local, mas nao atribui `send_buffer` nesse trecho. Dependendo da implementacao de `StatusCodes`, isso pode deixar a resposta de erro incompleta se nao for preenchida em outro ponto.

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

## 5.7 Registro no epoll

FDs registrados pelo `Client`:

- escrita do body: `pipe_in[1]` com `EPOLLOUT | EPOLLHUP`
- leitura da saida: `pipe_out[0]` com `EPOLLIN | EPOLLHUP`

Observacao: foi escolhido modo level-triggered (sem `EPOLLET`) para simplificar e evitar perda de evento por drenagem incompleta.

---

## 6. Integracao Com O Loop Do Server

No `Server::start()`:

1. `epoll_wait` devolve eventos.
2. Se FD for socket de escuta: aceita conexao.
3. Senao, procura em `clients`.
4. Se nao estiver em `clients`, tenta em `cgi_fd_map`.

Despacho para CGI:

- `EPOLLIN | EPOLLERR | EPOLLHUP` no FD de saida -> `handleCgiStdoutReadable`.
- `EPOLLOUT` no FD de entrada -> `handleCgiStdinWritable`.

Quando `processRequest` inicia CGI, `Server::handleClientData` registra os FDs dos pipes em `cgi_fd_map`.

Esse desenho separa claramente:

- mapa `clients`: sockets de rede.
- mapa `cgi_fd_map`: pipes internos de processo.

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

Observacao:

- o fluxo nao trata explicitamente erro de `write < 0` nesse metodo; uma falha permanente pode exigir tratamento adicional para evitar CGI preso.

---

## 8. Leitura Da Saida CGI (`handleCgiStdoutReadable`)

Contrato funcional:

- drenar stdout/stderr do CGI de forma incremental.
- detectar fim de stream.
- finalizar resposta quando nao houver mais dados.

Passos:

1. Loop de `read` em buffer de 8192 bytes enquanto `r > 0`.
2. Cada chunk e anexado em `cgi.output`.
3. Se `r == 0` (EOF):
   - remove FD do epoll,
   - fecha `pipe_out[0]`,
   - chama `finishCgiAndGenerateResponse`.
4. Se `EAGAIN`/`EWOULDBLOCK`:
   - usa `waitpid(pid, &status, WNOHANG)`.
   - se processo terminou, drena pipe novamente e finaliza.
5. Se outro erro de leitura:
   - loga erro (`strerror(errno)`).

Observacao de design:

- `stderr` e redirecionado para o mesmo pipe de `stdout`; isso melhora visibilidade de erro, mas pode poluir a resposta HTTP caso o script escreva logs em stderr junto com headers/body.

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

## 11. Limpeza E Encerramento Forcado

`cleanupCgiIfActive(epoll_fd)` e chamado quando cliente e fechado.

Acoes:

1. Se `cgi.pid > 0`, envia `SIGKILL` e chama `waitpid(..., WNOHANG)`.
2. Remove/fecha `pipe_in[1]` do epoll.
3. Remove/fecha `pipe_out[0]` do epoll.
4. `is_cgi_active = false`.

No `Server::closeClient(...)`:

- guarda FDs CGI antes de cleanup,
- faz cleanup,
- remove esses FDs de `cgi_fd_map`.

Isso evita dangling references no mapa de pipes.

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

1. Preencher `query/path` em `HttpRequest::parseRequestLine`.
   - ganho: `QUERY_STRING` e `SCRIPT_NAME` corretos para CGI.

2. Adicionar timeout de CGI por processo.
   - usar `cgi.start_time` + threshold e matar com `SIGKILL`/`SIGTERM`.

3. Tratar retorno de `write` em `handleCgiStdinWritable` com erro.
   - evitar stuck quando pipe de entrada falha.

4. Tratar status de saida do filho (`waitpid status`).
   - mapear falha real para `502/500` consistente.

5. Separar stderr de stdout (opcional).
   - melhora robustez do parsing HTTP.

6. Consolidar caminho CGI para nao depender de fallback em `Response`.
   - reduzir ambiguidade de arquitetura.

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

## 17. Resumo Executivo

A implementacao atual de CGI e event-driven e encaixada no mesmo loop `epoll` do servidor:

- `Client` inicia e controla o processo CGI com pipes non-blocking.
- `Server` despacha FDs de pipe em `cgi_fd_map`.
- `EnvBuilder` fornece ambiente para `execve`.
- resposta HTTP final e montada pelo proprio `Client` a partir da saida do script.

O desenho esta funcional para GET/POST CGI sem bloquear I/O global, mas ainda tem gaps importantes de robustez (timeout dedicado, parse de query/path, tratamento de erros de subprocesso e escrita parcial com falha).
