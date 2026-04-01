# 🏗️ ARQUITETURA DO WEBSERVER

**Data de Criação:** 1º de Abril de 2026  
**Última Atualização:** 1º de Abril de 2026  
**Status:** ✅ Documentação Completa

---

## 📋 Índice

1. [Visão Geral](#visão-geral)
2. [Componentes Principais](#componentes-principais)
3. [Fluxo de Requisição Completo](#fluxo-de-requisição-completo)
4. [Arquitetura em Camadas](#arquitetura-em-camadas)
5. [Padrões e Decisões de Design](#padrões-e-decisões-de-design)
6. [Decisões Críticas](#decisões-críticas)
7. [Recursos Implementados vs Não-Implementados](#recursos-implementados-vs-não-implementados)

---

## Visão Geral

O **Webserver** é um servidor HTTP/1.1 implementado em C++98 com as seguintes características principais:

- **Multiplexing de I/O** com `epoll` para suportar múltiplas conexões simultâneas
- **Máquina de estados** para cada cliente HTTP (READING_HEADERS → PROCESSING → SENDING_RESPONSE → DONE)
- **Arquitetura event-driven** não-bloqueante
- **Execução CGI não-bloqueante** com fork/pipes/execve
- **Configuração Nginx-like** para múltiplos virtual hosts e locations
- **Validações de segurança** contra path traversal, symlinks e uploads maliciosos

### Números-Chave

- **Sockets suportados:** Até 64 eventos simultâneos por iteração do epoll
- **Timeout de conexão:** 120 segundos
- **Timeout de CGI:** 60 segundos
- **Buffer de leitura:** 4096 bytes por recv()
- **Máximo de métodos HTTP:** 3 (GET, POST, DELETE)
- **Variáveis CGI:** 31+ variáveis de environment

---

## Componentes Principais

```
┌─────────────────────────────────────────────────────────────┐
│                      SERVIDOR HTTP                          │
│                  (main.cpp + sigaction)                      │
└──────────────────────┬──────────────────────────────────────┘
                       │
        ┌──────────────┼──────────────┐
        │              │              │
        v              v              v
    ┌────────────┐ ┌─────────────┐ ┌──────────────┐
    │ ConfigParser│ │Server.cpp   │ │Signal Handler│
    │(arquivos)  │ │(epoll loop) │ │(SIGINT,etc) │
    └────────────┘ └──────┬──────┘ └──────────────┘
                          │
         ┌────────────────┴────────────────┐
         │                                 │
         v                                 v
    ┌─────────────────────────────┐  ┌──────────────────┐
    │      Client Map [fd]        │  │ CGI FD Map       │
    │  - Cliente 1 (fd=5)         │  │ - pipe_out[0]=7  │
    │  - Cliente 2 (fd=6)         │  │ - pipe_in[1]=8   │
    │  - ...                      │  └──────────────────┘
    └──────┬──────────────────────┘
           │
      [A cada EPOLLIN/EPOLLOUT]
           │
      ┌────v────────────────────────────────┐
      │  Process HTTP Request                │
      │  ├─ HttpRequest::parse()            │
      │  ├─ Response::buildHttpResponse()   │
      │  ├─ (se CGI) Client::startCgi()     │
      │  └─ Client::sendData()              │
      └────────────────────────────────────┘
```

### 1. **Server** (include/Server.hpp, src/server/Server.cpp)

**Responsabilidade:** Orquestrador central - gerencia epoll, aceita conexões, despacha eventos

**Membros Chave:**
- `int epoll_fd` - File descriptor do epoll
- `std::map<int, Client*> clients` - Mapa de clientes por socket fd
- `std::map<int, Client*> cgi_fd_map` - Mapa de pipes de CGI
- `std::vector<ServerConfig> configs` - Configurações de servidores
- `epoll_event events[64]` - Buffer de eventos

**Métodos Principais:**
- `start()` - Loop principal (epoll_wait + handleEvent)
- `addClient(int fd)` - Cria novo cliente
- `handleClientData(int fd)` - Dados disponíveis para leitura
- `handleClientWrite(int fd)` - Socket pronto para escrita
- `handleCgiData(int cgi_fd)` - Dados de pipe CGI
- `closeClient(int fd)` - Cleanup completo de cliente
- `checkTimeout()` - Verifica e fecha clientes expirados

**Fluxo de Evento:**
```
epoll_wait(epoll_fd, events, 64, 1000)
  │
  ├─ EPOLLIN em fd cliente
  │   → Server::handleClientData(fd)
  │     → Client::appendRecvData()
  │     → Client::processRequest()
  │
  ├─ EPOLLOUT em fd cliente
  │   → Server::handleClientWrite(fd)
  │     → Client::sendData()
  │
  ├─ EPOLLIN em pipe CGI
  │   → Server::handleCgiData(cgi_fd)
  │     → Client::handleCgiStdoutReadable()
  │
  └─ EPOLLOUT em pipe CGI
      → Client::handleCgiStdinWritable()
```

---

### 2. **Client** (include/Client.hpp, src/server/Client.cpp)

**Responsabilidade:** Máquina de estados por conexão HTTP

**Estados (enum State):**
```cpp
READING_HEADERS   → Esperando \r\n\r\n
READING_BODY      → Recebendo POST body de acordo com Content-Length
PROCESSING        → Processando requisição (criar Response)
CGI_RUNNING       → Script CGI sendo executado (fork + pipes)
SENDING_RESPONSE  → Enviando resposta em chunks
DONE              → Resposta enviada, pronto para keep-alive
ERROR_413         → Payload Too Large detectado
```

**Fluxo de Estados:**
```
┌──────────────┐  recv() completa
│ READING_HEAD │──────────────────┐
│   ERS        │                  v
└──────────────┘  ┌───────────────────────┐
                  │ HttpRequest::parse()  │
                  │ (valida Content-Len)  │
                  └───────────┬───────────┘
        ┌─────(ERROR_413)─────┤
        │                     │ (não POST/PUT)
        │                     v
        │              ┌─────────────────┐
        │              │   PROCESSING    │
        │              └────────┬────────┘
        │                       │
        │        ┌──────────────┴────────────────┐
        │        │                               │
        │        v                               v
        │  ┌──────────────┐         ┌──────────────────┐
        │  │  (GET/POST   │         │  CGI_RUNNING     │
        │  │   DELETE)    │         │  (fork+pipes)    │
        │  │  Response OK │         │  (epoll children)│
        │  └──────┬───────┘         └────────┬─────────┘
        │         │                          │
        └────┬────┴──────┬───────────────────┘
             │           │
             v           v
    ┌─────────────────────────────┐
    │   SENDING_RESPONSE          │
    │   (send() em chunks)        │
    └─────────────┬───────────────┘
                  │
         (resposta completa + HTTP/1.1)
                  │
        ┌─────────v──────────┐
        │ Keep-Alive?        │
        └──┬───────────┬─────┘
           │ SIM       │ NÃO
           │           │
           v           v
        reset()     close()
        voltaEM  (fim conexão)
        READING_HEADERS
```

**Membros Chave:**
- `int fd` - File descriptor do socket
- `State state` - Estado atual
- `std::string recv_buffer` - Dados recebidos
- `HttpRequest *request` - Requisição parseada
- `Response *response` - Resposta a enviar
- `size_t send_offset` - Posição no envio
- `CgiState cgi` - Estado do CGI (pid, pipes, output)
- `time_t last_activity` - Timestamp do último evento

**Métodos Principais:**
- `appendRecvData(char*, size_t)` - Acumula dados recebidos
- `isRequestComplete()` - Verifica se headers + body estão completos
- `processRequest()` - Cria Response e inicia CGI se necessário
- `handleCgiStdoutReadable()` - Lê output do script CGI
- `handleCgiStdinWritable()` - Escreve body para script CGI
- `finishCgiAndGenerateResponse()` - Finaliza CGI e gera resposta
- `sendData()` - Envia resposta ao cliente
- `reset()` - Limpa para reutilizar (keep-alive)
- `closeClient()` - Cleanup completo

---

### 3. **HttpRequest** (include/HttpRequest.hpp, src/http/HttpRequest.cpp)

**Responsabilidade:** Parser incremental de HTTP/1.1

**Membros:**
- `std::string method` - GET, POST, DELETE, HEAD
- `std::string uri` - URI completo (/path?query#fragment)
- `std::string path` - Caminho sem query
- `std::string query_string` - Parâmetros de query
- `std::map<std::string, std::string> headers` - Headers HTTP
- `std::string body` - Corpo da requisição
- `std::string http_version` - HTTP/1.1, HTTP/1.0

**Métodos:**
- `parse(const std::string& data)` - Parse incremental
- `isComplete()` - Verifica se headers + body estão completos
- `getHeader(const std::string&)` - Busca header
- `getContentLength()` - Extrai Content-Length

**Fluxo de Parse:**
```
1. Buscar \r\n\r\n (fim de headers)
2. Parse request line: GET /path?query HTTP/1.1
3. Parse headers (chave: valor)
4. Validar Content-Length (se POST/PUT)
5. Acumular body até Content-Length bytes
6. Retornar isComplete() = true
```

---

### 4. **Response** (include/Response.hpp, src/http/Response.cpp)

**Responsabilidade:** Geração de respostas HTTP

**Fluxo:**
```
Response::Response(request, config)
  │
  └─ buildHttpResponse()
      │
      ├─ Validar path (path traversal, permissions)
      │
      ├─ [GET]
      │  ├─ Arquivo existe?
      │  │  ├─ Servir arquivo
      │  │  │
      │  │  └─ (se .cgi/.py/.php) → startCgi() no Client
      │  │
      │  └─ Diretório?
      │     ├─ Index file? (longest prefix match)
      │     │  ├─ Servir index
      │     │  │
      │     │  └─ (se .cgi/.py/.php) → startCgi()
      │     │
      │     └─ Autoindex on?
      │        ├─ Gerar HTML listing
      │        └─ Servir HTML
      │
      ├─ [POST]
      │  ├─ multipart/form-data?
      │  │  ├─ parseMultipartData()
      │  │  ├─ Validar extensão
      │  │  ├─ Sanitizar filename
      │  │  ├─ writeFileToDisk()
      │  │  └─ Retornar resposta
      │  │
      │  └─ Redirecionamento?
      │     ├─ 301/302/307 com Location header
      │
      └─ [DELETE]
         ├─ isPathSafe() validação
         ├─ unlink(file)
         └─ 204 No Content
```

**Validações de Segurança (5 camadas):**
1. **Path traversal:** Detecta `..` e resolve symlinks
2. **Symlink checking:** realpath() vs original path
3. **File permissions:** Verifica se arquivo é legível/deletável
4. **Protected files:** Whitelist de arquivos não-deletáveis
5. **Extension whitelist:** Apenas extensões permitidas podem ser uploaded

---

### 5. **ConfigParser** (include/ConfigParser.hpp, src/config/ConfigParser.cpp)

**Responsabilidade:** Parse de arquivos de configuração `.conf`

**Estruturas de Dados:**

```cpp
struct ServerConfig {
    std::string interface;              // 0.0.0.0, localhost
    int port;                           // 8080
    std::string server_name;            // "example.com"
    std::string root;                   // "www"
    std::map<int, std::string> error_pages; // {404: "/404.html"}
    std::vector<std::string> index_files; // ["index.html", "index.php"]
    size_t client_max_body_size;        // Limite upload
    std::vector<LocationConfig> locations;
};

struct LocationConfig {
    std::string path;                       // "/api"
    std::vector<std::string> allowed_methods; // ["GET", "POST"]
    std::string root;                       // Override de raiz
    bool autoindex;                         // Directory listing?
    std::vector<std::string> index_files;   // Override de índices
    std::string cgi_path;                   // "/usr/bin/python3"
    int redirect_code;                      // 301, 302, 307
    std::string redirect_url;               // "/new-path"
    std::map<std::string, std::string> cgi_handlers; // {".py": "/usr/bin/python3"}
    size_t client_max_body_size;            // Override de tamanho
    std::string upload_dir;                 // Diretório de uploads
};
```

**Exemplo de Configuração:**

```nginx
server {
    listen 127.0.0.1:8080;
    server_name localhost;
    root www;
    
    client_max_body_size 10M;
    
    error_page 404 /errors/404.html;
    error_page 500 /errors/500.html;
    
    index index.html index.php;
    
    location / {
        allowed_methods GET POST DELETE;
        autoindex off;
    }
    
    location /cgi-bin {
        root www/cgi-bin;
        allowed_methods GET POST;
        
        cgi_extension .py;
        cgi_path /usr/bin/python3;
        
        cgi_extension .php;
        cgi_path /usr/bin/php-cgi;
    }
    
    location /uploads {
        allowed_methods POST;
        upload_dir www/uploads;
    }
}
```

---

### 6. **EnvBuilder** (include/EnvBuilder.hpp, src/cgi/EnvBuilder.cpp)

**Responsabilidade:** Construir variáveis de environment para execução CGI

**31+ Variáveis Geradas:**
```cpp
REQUEST_METHOD      - GET, POST, DELETE, HEAD
SCRIPT_NAME         - /cgi-bin/script.py
SCRIPT_FILENAME     - /absolute/path/to/script.py
QUERY_STRING        - key1=value1&key2=value2
CONTENT_TYPE        - multipart/form-data; boundary=...
CONTENT_LENGTH      - tamanho do body
HTTP_HOST           - localhost:8080
HTTP_USER_AGENT     - curl/7.68.0
HTTP_ACCEPT         - */*
... (25+ mais)
```

---

### 7. **StatusCodes** (include/StatusCodes.hpp, src/http/StatusCodes.cpp)

**Responsabilidade:** Geração de headers e mensagens de status HTTP

**Funções:**
- `http200OK(body, content_type)` - 200 OK
- `http201Created()` - 201 Created (upload bem-sucedido)
- `http204NoContent()` - 204 No Content (DELETE bem-sucedido)
- `http301MovedPermanently(location)` - 301 redirect
- `http302Found(location)` - 302 redirect
- `http400BadRequest()` - 400 Bad Request
- `http403Forbidden()` - 403 Forbidden
- `http404NotFound()` - 404 Not Found
- `http405MethodNotAllowed()` - 405 Method Not Allowed
- `http409Conflict()` - 409 Conflict
- `http413PayloadTooLarge()` - 413 Payload Too Large
- `http500InternalServerError()` - 500 Internal Server Error
- `http502BadGateway()` - 502 Bad Gateway (CGI error)
- `http503ServiceUnavailable()` - 503 Service Unavailable

---

### 8. **FileUtils** (include/FileUtils.hpp, src/utils/FileUtils.cpp)

**Responsabilidade:** Operações de arquivo seguras

**Funções:**
- `readFile(filepath)` - Lê arquivo completo (com types)
- `writeFileToDisk(filepath, content)` - Escreve arquivo
- `findIndexFile(directory, index_list)` - Busca index com prefix matching
- `isFileReadable(filepath)` - Verifica permissões
- `getFileSize(filepath)` - Tamanho do arquivo
- `getMimeType(filepath)` - Detecta MIME type

---

### 9. **StringUtils** (include/StringUtils.hpp, src/utils/StringUtils.cpp)

**Responsabilidade:** Manipulação segura de strings

**Funções:**
- `trim(str)` - Remove espaços
- `split(str, delimiter)` - Divide string
- `replaceAll(str, from, to)` - Substitui substrings
- `toLower(str)` - Conversor para minúsculas
- `isNumber(str)` - Validação numérica
- `encodeHtml(str)` - Escapa caracteres HTML

---

## Fluxo de Requisição Completo

### 1. Aceitar Conexão

```
[TCP Connect]
    ↓
Server::newConnection(int client_fd)
    ├─ fcntl(O_NONBLOCK)
    ├─ epoll_ctl(ADD, client_fd, EPOLLIN)
    └─ clients[client_fd] = new Client()
        └─ estado = READING_HEADERS
```

### 2. Receber e Parsear Headers

```
[EPOLLIN em client_fd]
    ↓
Server::handleClientData(client_fd)
    ├─ recv(client_fd, buffer, 4096)
    └─ Client::appendRecvData(buffer, nread)
        │
        └─ HttpRequest::isComplete()?
           ├─ SIM: apagar \r\n\r\n encontrado
           ├─ Parse request line (GET /path HTTP/1.1)
           ├─ Parse headers
           └─ Validar Content-Length vs client_max_body_size
              ├─ Excedido: estado = ERROR_413
              └─ OK: estado = READING_BODY (se POST) ou PROCESSING
```

### 3. Receber Body (POST)

```
[Se estado = READING_BODY]
    ↓
[EPOLLIN em client_fd]
    ├─ recv() acumula até Content-Length
    └─ Quando body completo:
        └─ estado = PROCESSING
```

### 4. Processar Requisição

```
Client::processRequest()
    ├─ Response response(request, config)
    │   ├─ buildHttpResponse()
    │   │   ├─ Validar path (path traversal)
    │   │   ├─ Encontrar location match
    │   │   │
    │   │   ├─ [GET ou HEAD]
    │   │   │   ├─ Arquivo? → readFile() + http200OK()
    │   │   │   ├─ Diretório?
    │   │   │   │   ├─ Index file? → http200OK(index)
    │   │   │   │   ├─ Autoindex on? → generateDirectoryListing()
    │   │   │   │   └─ Senão → http403Forbidden()
    │   │   │   └─ Extensão CGI? → return sem response (será CGI)
    │   │   │
    │   │   ├─ [POST]
    │   │   │   ├─ multipart/form-data?
    │   │   │   │   ├─ parseMultipartData()
    │   │   │   │   ├─ Validar extensão
    │   │   │   │   ├─ writeFileToDisk()
    │   │   │   │   └─ http201Created()
    │   │   │   │
    │   │   │   ├─ application/x-www-form-urlencoded?
    │   │   │   │   └─ parseFormData()
    │   │   │   │
    │   │   │   └─ application/json?
    │   │   │       └─ parseJsonData()
    │   │   │
    │   │   ├─ [DELETE]
    │   │   │   ├─ Validar path
    │   │   │   ├─ Is protected file?
    │   │   │   │   └─ http403Forbidden()
    │   │   │   ├─ remove(filepath)
    │   │   │   └─ http204NoContent()
    │   │   │
    │   │   └─ [Redirecionamento?]
    │   │       └─ http301/302/307(location)
    │
    ├─ **SE ARQUIVO NORMAL:**
    │   └─ estado = SENDING_RESPONSE
    │
    └─ **SE CGI:**
        ├─ Client::startCgi()
        │   ├─ Encontra interpretador na config
        │   ├─ pipe(cgi.pipe_in)  - entrada
        │   ├─ pipe(cgi.pipe_out) - saída
        │   ├─ fork()
        │   │   ├─ **[FILHO]**
        │   │   │   ├─ dup2(pipe_in[0], STDIN)
        │   │   │   ├─ dup2(pipe_out[1], STDOUT)
        │   │   │   ├─ close() pipes desnecessários
        │   │   │   ├─ EnvBuilder::buildEnv() - 31+ variáveis
        │   │   │   ├─ chdir(script_dir)
        │   │   │   └─ execve(interpretador, args, env)
        │   │   │       └─ (carrega script.py, script.php, etc)
        │   │   │
        │   │   └─ **[PAI]**
        │   │       ├─ close(pipe_in[0], pipe_out[1])
        │   │       ├─ epoll_ctl(ADD, pipe_in[1], EPOLLOUT)
        │   │       ├─ epoll_ctl(ADD, pipe_out[0], EPOLLIN)
        │   │       ├─ cgi_fd_map[pipe_in[1]] = client
        │   │       ├─ cgi_fd_map[pipe_out[0]] = client
        │   │       └─ estado = CGI_RUNNING
        │   │           └─ timeout = now + 60s
        │   │
        │   └─ **[EPOLLOUT em pipe_in]**
        │       ├─ Client::handleCgiStdinWritable()
        │       ├─ Escrever request body em chunks
        │       └─ Quando done, close(pipe_in[1])
        │
        └─ **[EPOLLIN em pipe_out]**
            ├─ Client::handleCgiStdoutReadable()
            ├─ Ler output do script em chunks
            ├─ Acumular em cgi.output
            └─ **[EPOLLHUP ou error]**
                ├─ waitpid(cgi.pid, &status) - recolhe processo
                ├─ Parse output CGI
                ├─ Gerar Response com CGI output
                └─ estado = SENDING_RESPONSE
```

### 5. Enviar Resposta

```
estado = SENDING_RESPONSE
    ├─ [EPOLLOUT em client_fd]
    │   ├─ Client::sendData()
    │   ├─ send(client_fd, response_str[send_offset:])
    │   ├─ send_offset += bytes_sent
    │   │
    │   └─ **Se response completa:**
    │       ├─ HTTP/1.1 + Keep-Alive?
    │       │   ├─ Client::reset() - limpa buffers
    │       │   ├─ estado = READING_HEADERS
    │       │   └─ [REUTILIZA CONEXÃO]
    │       │
    │       └─ Senão:
    │           ├─ Server::closeClient(client_fd)
    │           ├─ Client::cleanupCgiIfActive()
    │           ├─ Client::closeClient()
    │           └─ delete client
    │
    └─ Continua até completion ou timeout
```

---

## Arquitetura em Camadas

```
┌────────────────────────────────────────────┐
│         APLICAÇÃO (main.cpp)               │
│     - Signal handlers (SIGINT, SIGPIPE)    │
│     - Inicialização e cleanup              │
└────────────────┬───────────────────────────┘
                 │
┌────────────────v───────────────────────────┐
│         SERVIDOR (Server)                  │
│     - epoll multiplexing                   │
│     - Gerenciamento de conexões            │
│     - Despacho de eventos                  │
└────────────────┬───────────────────────────┘
                 │
    ┌────────────┴─────────────┐
    │                          │
    v                          v
┌──────────────────┐  ┌────────────────────┐
│  CLIENTE (Client)│  │ CGI (Fork/Pipes)  │
│ - Máquina estados│  │ - EnvBuilder      │
│ - HTTP parsing   │  │ - Non-blocking I/O│
│ - Buffer gerencm │  │ - Timeout         │
└────────┬─────────┘  └────────┬──────────┘
         │                     │
    ┌────v──────────────────────v────┐
    │  PARSING HTTP (HttpRequest)     │
    │ - Request line, headers, body   │
    │ - Content-Length validation     │
    └────┬──────────────────────┬─────┘
         │                      │
    ┌────v────┐            ┌────v─────┐
    │RESPONSE │            │  CONFIG   │
    │ - GET   │            │ - Parse   │
    │ - POST  │            │ - Validate│
    │ -DELETE │            │ - Locations│
    └────┬────┘            └────┬─────┘
         │                      │
    ┌────v──────────────────────v────┐
    │  UTILITÁRIOS (FileUtils, etc)   │
    │ - Leitura/escrita segura        │
    │ - Manipulação de strings        │
    │ - MIME types                    │
    └────────────────────────────────┘
```

---

## Padrões e Decisões de Design

### 1. **Máquina de Estados por Cliente**
- **Razão:** Cada cliente pode estar em diferentes fases (recebendo, processando, enviando)
- **Benefício:** Código limpo com transições bem-definidas
- **Implementação:** Enum `State` em Client com métodos por estado

### 2. **Dois Mapas (clients + cgi_fd_map)**
- **Razão:** Sockets TCP e pipes CGI precisam de tratamento diferente
- **Benefício:** Eventos de CGI podem ser multiplexados juntamente com clientes
- **Implementação:** `map<int, Client*>` para associar FD → Cliente

### 3. **Non-Blocking I/O + epoll**
- **Razão:** Suportar múltiplas conexões em thread único
- **Benefício:** Escalabilidade, sem overhead de threads
- **Implementação:** fcntl(O_NONBLOCK) + epoll_wait() loop

### 4. **CGI como Fork Filho**
- **Razão:** Segurança e isolamento de processo
- **Benefício:** Script defeituoso não derruba servidor
- **Implementação:** fork() + execve() + pipes para I/O

### 5. **Parsing Incremental de Requisições**
- **Razão:** Suportar bodies grandes sem buffer fixo
- **Benefício:** Economia de memória para uploads
- **Implementação:** Acumular recv_buffer até \r\n\r\n

### 6. **Longest Prefix Matching para Locations**
- **Razão:** Nginx-compatible routing
- **Benefício:** Rotas mais específicas têm prioridade
- **Implementação:** vector de locations, procura por maior match

### 7. **Validação em Múltiplas Camadas**
- **Razão:** Defesa em profundidade contra ataques
- **Benefício:** Múltiplos problemas de segurança evitados
- **Implementação:** Path traversal → Permissions → Whitelist → Sanitização

---

## Decisões Críticas

### Por que epoll e não poll?

**poll():**
- ✓ Portable
- ✗ O(n) - escaneia todos FDs cada iteração
- ✗ Lento com 100+ connections

**epoll:**
- ✓ O(1) - apenas eventos ativos
- ✓ Muito mais rápido
- ✗ Linux-only (aceitável para projeto educacional)

**Decisão:** epoll (performance e escalabilidade)

---

### Por que fork() para CGI e não threads?

**Threads:**
- ✗ Compartilham memória (race conditions)
- ✗ Mais complexo sincronizar
- ✗ Um script defeituoso pode derruba server

**fork():**
- ✓ Isolamento completo de memória
- ✓ Sem race conditions
- ✓ Script pode fazer qualquer coisa sem afetar pai
- ✓ Padrão de facto (como Apache)

**Decisão:** fork() (segurança e simplicidade)

---

### Por que Nginx-like config e não Apache?

**Apache httpd.conf:**
- ✓ Mais configurável
- ✗ Muito mais complexo
- ✗ Bloated para este escopo

**Nginx:**
- ✓ Simples e limpo
- ✓ Perfeito para este projeto
- ✓ Familiar a desenvolvedores modernos

**Decisão:** Nginx-like (simplicidade e relevância)

---

## Recursos Implementados vs Não-Implementados

### ✅ IMPLEMENTADOS (Baseline Completo)

**Métodos HTTP:**
- ✔ GET - Servir arquivos, directory listing, CGI
- ✔ POST - Uploads multipart, form data
- ✔ DELETE - Remoção de arquivos com validação
- ✔ HEAD - GET sem body

**HTTP/1.1:**
- ✔ Request parsing (method, URI, headers, body)
- ✔ Response headers (Content-Type, Content-Length, Date, etc)
- ✔ Keep-Alive com reset() de estado
- ✔ Status codes (2xx, 3xx, 4xx, 5xx)

**Funcionalidade:**
- ✔ Múltiplos virtual hosts (server_name)
- ✔ Locations com routing
- ✔ CGI com 31+ variáveis
- ✔ Upload com sanitização
- ✔ Directory listing com sorting
- ✔ Error pages customizadas
- ✔ Redirecionamentos (301/302/307)
- ✔ MIME type detection

**Segurança:**
- ✔ Path traversal protection
- ✔ Symlink resolution
- ✔ File permissions check
- ✔ Extension whitelist para uploads
- ✔ Filename sanitization
- ✔ Content-Length validation

**Performance:**
- ✔ Non-blocking I/O com epoll
- ✔ Timeouts (120s conexão, 60s CGI)
- ✔ Chunked sending
- ✔ Buffer reutilizável

---

### ❌ NÃO IMPLEMENTADOS (Fora do Escopo)

**Protocolos:**
- ✘ HTTPS/TLS (requires OpenSSL)
- ✘ HTTP/2
- ✘ WebSockets

**Headers:**
- ✘ Chunked Transfer Encoding (Transfer-Encoding: chunked)
- ✘ Range Requests (206 Partial Content)
- ✘ CORS (Access-Control-* headers)
- ✘ Compression (Content-Encoding: gzip)

**Autenticação:**
- ✘ Basic Auth
- ✘ Digest Auth
- ✘ OAuth2

**CGI Avançado:**
- ✘ FastCGI
- ✘ WSGI
- ✘ Spawn-fcgi

**Performance Avançada:**
- ✘ sendfile() (zero-copy)
- ✘ AIO (async I/O)
- ✘ Connection pooling
- ✘ Load balancing

**Operação:**
- ✘ Logging estruturado (access/error logs)
- ✘ Graceful reload (SIGHUP)
- ✘ Hot restart
- ✘ Stats/metrics

---

## Conclusão

O webserver implementa um servidor HTTP/1.1 funcional e robusto com:
- **Arquitetura limpa** baseada em máquina de estados
- **Multiplexing eficiente** com epoll
- **CGI seguro** via fork/pipes
- **Validações rigorosas** contra ataques
- **Configuração flexível** estilo Nginx
- **Suporte a múltiplos hosts** e locations

É um excelente projeto educacional para entender programação de sistemas, redes e arquitetura de servidores web reais.
