# 🔍 Sistema de Parsing do Webserver

## 📅 Data de Criação
**19 de Fevereiro de 2026**

### 📝 Última Atualização
**2 de Março de 2026** - Análise implementação com ConfigValidator e error handling

---

## 🎯 Visão Geral

O webserver implementa **dois sistemas de parsing principais**:

1. **ConfigParser** - Parse de arquivos de configuração (estilo Nginx)
2. **HttpRequest** - Parse de requisições HTTP/1.1

Além disso, utiliza **utilitários auxiliares**:
- **ConfigHelper** - Funções especializadas de parse de configuração
- **ConfigValidator** - 🆕 **Validação robusta durante o parse**
- **StringUtils** - Manipulação de strings

---

## 📑 Índice

1. [ConfigParser - Parse de Configuração](#1-configparser---parse-de-configuração)
2. [HttpRequest - Parse HTTP](#2-httprequest---parse-http)
3. [ConfigHelper - Funções Auxiliares](#3-confighelper---funções-auxiliares)
4. [StringUtils - Utilidades](#4-stringutils---utilidades)
5. [Comparação e Boas Práticas](#5-comparação-e-boas-práticas)

---

## 1. ConfigParser - Parse de Configuração

### 📁 Arquivos
- **Header:** `include/ConfigParser.hpp`
- **Implementação:** `src/config/ConfigParser.cpp`
- **Validação:** `include/ConfigValidator.hpp` (🆕)

### 🎯 Propósito

Parse de arquivos de configuração no **formato Nginx-like**, convertendo texto em estruturas C++.

### 📊 Estruturas de Dados

#### **LocationConfig**
Configuração específica de uma location (rota).

```cpp
struct LocationConfig {
    std::string path;                       // Caminho da location (/api, /uploads)
    std::vector<std::string> allowed_methods; // GET, POST, DELETE
    std::string root;                       // Diretório raiz
    bool autoindex;                         // Listagem de diretórios (default: false)
    std::vector<std::string> index_files;   // index.html, index.php
    std::string cgi_path;                   // Caminho do interpretador CGI
    int redirect_code;                      // Código de redirecionamento (default: 0)
    std::string redirect_url;               // URL de destino
    std::vector<std::string> cgi_extensions;// .php, .py
    size_t client_max_body_size;            // Tamanho máximo do body (default: 0)
    std::string upload_dir;                 // Diretório de uploads 🆕
    
    LocationConfig() : autoindex(false), 
                       redirect_code(0), 
                       client_max_body_size(0) {}
};
```

### 🆕 Campos Novos
- **`upload_dir`**: Diretório específico para uploads da location

#### **ServerConfig**
Configuração de um bloco server.

```cpp
struct ServerConfig {
    std::string interface;                       // Interface (0.0.0.0, localhost) 🆕
    int port;                                    // Porta (8080)
    std::string server_name;                     // Nome do servidor
    std::string root;                            // Diretório raiz
    std::map<std::string, std::string> error_pages; // 404 -> /errors/404.html
    std::vector<std::string> index_files;        // Arquivos índice
    size_t client_max_body_size;                 // Limite de body
    std::vector<LocationConfig> locations;       // Locations do servidor
    
    ServerConfig() : port(8080), 
                     server_name("localhost"),
                     root("www"),
                     client_max_body_size(0) {}
};
```

### 🆕 Campos Novos
- **`interface`**: A partir de `listen interface:port` (ex: `127.0.0.1:8080`)

### 📝 Exemplo de Arquivo de Configuração

```nginx
# config/default.conf
server {
    listen 8080;
    server_name localhost;
    root www;
    index index.html index.htm;
    
    error_page 404 /errors/404.html;
    error_page 500 /errors/500.html;
    
    client_max_body_size 10M;
    
    location /api {
        root /var/www/api;
        allowed_methods GET POST;
        autoindex off;
    }
    
    location /uploads {
        root /var/www/uploads;
        client_max_body_size 100M;
        autoindex on;
    }
}
```

### 🔄 Fluxo de Parsing

```
┌────────────────────────────────────────────┐
│   ConfigParser::loadFromFile(filename)     │
└──────────────────┬─────────────────────────┘
                   │
                   v
         ┌─────────────────┐
         │ Abrir arquivo   │
         └────────┬────────┘
                  │
                  v
    ┌─────────────────────────┐
    │ Ler linha por linha     │
    └────────┬────────────────┘
             │
    ┌────────v────────┐
    │ Trim + Ignorar  │
    │ comentários (#) │
    └────────┬────────┘
             │
    ┌────────v──────────┐
    │ Encontrou "server"│
    └────────┬──────────┘
             │ SIM
             v
    ┌──────────────────────┐
    │ parseServerBlock()   │
    └────────┬─────────────┘
             │
    ┌────────v─────────────────┐
    │ Parse de diretivas:      │
    │ - listen                 │
    │ - server_name            │
    │ - root                   │
    │ - error_page             │
    │ - client_max_body_size   │
    └────────┬─────────────────┘
             │
    ┌────────v──────────┐
    │ Encontrou         │
    │ "location"?       │
    └────────┬──────────┘
             │ SIM
             v
    ┌──────────────────────┐
    │ parseLocationBlock() │
    └────────┬─────────────┘
             │
    ┌────────v──────────┐
    │ Parse diretivas   │
    │ da location       │
    └────────┬──────────┘
             │
    ┌────────v──────────┐
    │ Adicionar ao      │
    │ server.locations  │
    └────────┬──────────┘
             │
    ┌────────v──────────┐
    │ Continuar até "}" │
    └────────┬──────────┘
             │
    ┌────────v──────────┐
    │ Adicionar server  │
    │ ao vetor servers  │
    └────────┬──────────┘
             │
             v
    ┌──────────────┐
    │   Retornar   │
    └──────────────┘
```

### ⚡ Estratégia de Validação (🆕)

**Antes:** Silent fail com valores default
**Agora:** ✅ Validação robusta com **exceptions em tempo de parse**

```cpp
// ConfigParser valida DURANTE o parse
if (ConfigValidator::validatePort(value))
    server.port = atoi(value.c_str());
else
    throw std::runtime_error("invalid port: " + value);
```

**Benefícios:**
- ✅ Erros detectados no startup (antes de aceitar conexões)
- ✅ Mensagens de erro claras
- ✅ Previne configurações inválidas
- ✅ Falha rápido e com diagnostico

### 🛠️ Métodos Principais

#### **loadFromFile()**

```cpp
bool ConfigParser::loadFromFile(const std::string& filename);
```

**Responsabilidade:** Carrega e faz parse do arquivo de configuração completo.

**Algoritmo:**
```cpp
1. Abrir arquivo
2. Para cada linha:
   a. Fazer trim (remover espaços)
   b. Ignorar linhas vazias e comentários (#)
   c. Se encontrar "server {":
      - Chamar parseServerBlock()
      - Adicionar ao vetor servers
3. Validar se há pelo menos um servidor
4. Retornar sucesso/falha
```

**Exemplo de uso:**
```cpp
ConfigParser config;
if (!config.loadFromFile("config/default.conf")) {
    std::cerr << "Erro ao carregar configuração" << std::endl;
}
```

---

#### **parseServerBlock()**

```cpp
void ConfigParser::parseServerBlock(std::ifstream& file, ServerConfig& server);
```

**Responsabilidade:** Parse de um bloco `server { ... }`.

**Diretivas suportadas:**

| Diretiva | Exemplo | Ação |
|----------|---------|------|
| `listen` | `listen 8080;` | Define porta |
| `server_name` | `server_name localhost;` | Nome do servidor |
| `root` | `root www;` | Diretório raiz |
| `index` | `index index.html;` | Arquivos índice |
| `error_page` | `error_page 404 /404.html;` | Páginas de erro |
| `client_max_body_size` | `client_max_body_size 10M;` | Limite de body |
| `location` | `location /api { ... }` | Bloco location |

**Algoritmo:**
```cpp
1. Ler linha por linha até encontrar "}"
2. Para cada linha:
   a. Trim
   b. Ignorar comentários
   c. Remover ponto-e-vírgula final
   d. Extrair key e value
   e. Se key == "listen": server.port = atoi(value)
   f. Se key == "server_name": server.server_name = value
   g. Se key == "location": parseLocationBlock()
   h. Senão: ConfigHelper::parseCommonConfig()
```

**Tratamento especial de `location`:**
```cpp
if (key == "location") {
    LocationConfig location;
    std::string path;
    
    // Extrair caminho: "location /api {"
    std::istringstream lss(value);
    lss >> path;
    
    // Remover "{" se presente
    size_t bracePos = path.find('{');
    if (bracePos != std::string::npos)
        path = path.substr(0, bracePos);
    
    location.path = path;
    parseLocationBlock(file, location);
    server.locations.push_back(location);
}
```

---

#### **parseLocationBlock()**

```cpp
void ConfigParser::parseLocationBlock(std::ifstream& file, LocationConfig& location);
```

**Responsabilidade:** Parse de um bloco `location { ... }`.

**Diretivas suportadas:**

| Diretiva | Exemplo | Ação |
|----------|---------|------|
| `autoindex` | `autoindex on;` | Habilita listagem |
| `return` | `return 301 /new-url;` | Redirecionamento |
| `root` | `root /var/www;` | Diretório raiz |
| `index` | `index index.php;` | Arquivos índice |
| `allowed_methods` | `allowed_methods GET POST;` | Métodos permitidos |

**Algoritmo:**
```cpp
1. Ler até encontrar "}"
2. Para cada linha:
   a. Trim e ignorar comentários
   b. Remover ";"
   c. Extrair key e value
   d. Se key == "autoindex": parseAutoIndex()
   e. Se key == "return": parseRedirect()
   f. Senão: parseCommonConfig() para location
```

---

### 🔍 Detalhes de Implementação

#### **Leitura de Arquivo**
```cpp
std::ifstream file;
std::string line;

file.open(filename.c_str());
if (!file.is_open())
    throw std::runtime_error("Error opening file");

while (std::getline(file, line)) {
    // Processar linha
}
file.close();
```

#### **Trim e Comentários**
```cpp
line = StringUtils::trim(line);

// Ignorar linhas vazias
if (line.empty())
    continue;

// Ignorar comentários
if (line[0] == '#')
    continue;
```

#### **Remover Ponto-e-Vírgula**
```cpp
if (!line.empty() && line[line.size() - 1] == ';')
    line.erase(line.size() - 1);
```

#### **Extração de Key-Value**
```cpp
std::string key, value;
std::istringstream iss(line);

iss >> key;              // Primeira palavra = key
std::getline(iss, value); // Resto = value
```

---

## 2. HttpRequest - Parse HTTP

### 📁 Arquivos
- **Header:** `include/HttpRequest.hpp`
- **Implementação:** `src/http/HttpRequest.cpp`

### 🎯 Propósito

Parse de requisições HTTP/1.1, convertendo texto raw em estrutura C++.

### 📊 Estrutura de Dados

```cpp
class HttpRequest {
private:
    std::string method;                        // GET, POST, DELETE
    std::string uri;                           // /index.html, /api/users
    std::string version;                       // HTTP/1.1
    std::map<std::string, std::string> headers; // Host, Content-Length, etc.
    std::string body;                          // Corpo da requisição
};
```

### 📝 Exemplo de Requisição HTTP

```http
GET /api/users HTTP/1.1
Host: localhost:8080
User-Agent: Mozilla/5.0
Accept: text/html
Connection: keep-alive
Content-Length: 0

```

```http
POST /api/users HTTP/1.1
Host: localhost:8080
Content-Type: application/json
Content-Length: 45

{"name": "João Silva", "email": "joao@test.com"}
```

### 🔄 Estrutura de uma Requisição HTTP

```
┌─────────────────────────────────────────────┐
│           REQUEST LINE                      │
│  GET /index.html HTTP/1.1                   │
├─────────────────────────────────────────────┤
│           HEADERS                           │
│  Host: localhost:8080                       │
│  User-Agent: curl/7.68.0                    │
│  Accept: */*                                │
│  Connection: keep-alive                     │
│  Content-Length: 25                         │
├─────────────────────────────────────────────┤
│         BLANK LINE (\r\n\r\n)               │
├─────────────────────────────────────────────┤
│           BODY (opcional)                   │
│  {"key": "value"}                           │
└─────────────────────────────────────────────┘
```

### 🔄 Fluxo de Parsing

```
┌──────────────────────────────────┐
│  HttpRequest::parse(raw_request) │
└────────────┬─────────────────────┘
             │
             v
   ┌─────────────────┐
   │ Criar stream    │
   │ istringstream   │
   └────────┬────────┘
            │
            v
   ┌────────────────────┐
   │ getline() primeira │
   │ linha              │
   └────────┬───────────┘
            │
            v
   ┌────────────────────┐
   │ parseRequestLine() │
   │                    │
   │ GET /path HTTP/1.1 │
   │  ↓    ↓      ↓     │
   │ method uri version │
   └────────┬───────────┘
            │
            v
   ┌────────────────────┐
   │ parseHeaders()     │
   │                    │
   │ Host: localhost    │
   │   ↓        ↓       │
   │  key     value     │
   └────────┬───────────┘
            │ (até linha vazia)
            v
   ┌────────────────────┐
   │ parseBody()        │
   │                    │
   │ Resto do stream    │
   └────────┬───────────┘
            │
            v
   ┌────────────────────┐
   │ Retornar HttpRequest│
   └────────────────────┘
```

### ⚡ Estratégia de Validação (🆕)

**Antes:** Silent fail com valores default
**Agora:** ✅ Validação robusta com **exceptions em tempo de parse**

```cpp
// ConfigParser valida DURANTE o parse
if (ConfigValidator::validatePort(value))
    server.port = atoi(value.c_str());
else
    throw std::runtime_error("invalid port: " + value);
```

**Benefícios:**
- ✅ Erros detectados no startup (antes de aceitar conexões)
- ✅ Mensagens de erro claras
- ✅ Previne configurações inválidas
- ✅ Falha rápido e com diagnostico

### 🛠️ Métodos Principais

#### **parse() - Método Estático**

```cpp
static HttpRequest parse(const std::string& raw_request);
```

**Responsabilidade:** Ponto de entrada principal para parsing.

**Algoritmo:**
```cpp
HttpRequest parse(const std::string& raw_request) {
    HttpRequest req;
    std::istringstream stream(raw_request);
    std::string line;

    // 1. Parse request line
    if (std::getline(stream, line)) {
        // Remover \r se presente (Windows)
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        req.parseRequestLine(line);
    }

    // 2. Parse headers
    req.parseHeaders(stream);

    // 3. Parse body
    req.parseBody(stream);

    return req;
}
```

**Uso:**
```cpp
std::string raw = "GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n";
HttpRequest request = HttpRequest::parse(raw);
```

---

#### **parseRequestLine()**

```cpp
void HttpRequest::parseRequestLine(const std::string& line);
```

**Responsabilidade:** Parse da primeira linha (Request Line).

**Formato:** `METHOD URI VERSION`

**Algoritmo:**
```cpp
void parseRequestLine(const std::string& line) {
    std::istringstream iss(line);
    
    // Extrair as três palavras
    iss >> this->method >> this->uri >> this->version;
    
    // Default para /index.html se for /
    if (this->uri == "/")
        this->uri = "/index.html";
    
    std::cout << "[REQUEST] " << method << " " 
              << uri << " " << version << std::endl;
}
```

**Exemplos:**
```
Input:  "GET /index.html HTTP/1.1"
Output: method="GET", uri="/index.html", version="HTTP/1.1"

Input:  "POST /api/users HTTP/1.1"
Output: method="POST", uri="/api/users", version="HTTP/1.1"

Input:  "GET / HTTP/1.1"
Output: method="GET", uri="/index.html", version="HTTP/1.1"  (default)
```

---

#### **parseHeaders()**

```cpp
void HttpRequest::parseHeaders(std::istringstream& stream);
```

**Responsabilidade:** Parse de todos os headers HTTP.

**Formato:** `Key: Value`

**Algoritmo:**
```cpp
void parseHeaders(std::istringstream& stream) {
    std::string line;
    
    while (std::getline(stream, line)) {
        // Remover \r
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        
        // Linha vazia = fim dos headers
        if (line.empty())
            break;
        
        // Procurar ":"
        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string key = line.substr(0, colon_pos);
            std::string value = line.substr(colon_pos + 1);
            
            // Trim em key e value
            key = StringUtils::trim(key);
            value = StringUtils::trim(value);
            
            this->headers[key] = value;
        }
    }
}
```

**Exemplo:**
```
Input:
Host: localhost:8080
Content-Type: text/html
Content-Length: 123

Output (headers map):
{
    "Host" -> "localhost:8080",
    "Content-Type" -> "text/html",
    "Content-Length" -> "123"
}
```

**Detecção de fim dos headers:**
```
Headers terminam com linha vazia (\r\n\r\n)
```

---

#### **parseBody()**

```cpp
void HttpRequest::parseBody(std::istringstream& stream);
```

**Responsabilidade:** Capturar todo o corpo da requisição (POST, PUT).

**Algoritmo:**
```cpp
void parseBody(std::istringstream& stream) {
    std::string line;
    std::ostringstream body_stream;
    
    // Ler todas as linhas restantes
    while (std::getline(stream, line)) {
        // Remover \r
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        body_stream << line << "\n";
    }
    
    this->body = body_stream.str();
    
    // Remover \n final
    if (!body.empty() && body[body.size() - 1] == '\n')
        body.erase(body.size() - 1);
}
```

**Exemplo:**
```
Input POST:
POST /api/data HTTP/1.1
Content-Length: 27

{"name":"test","age":25}

Output:
body = "{\"name\":\"test\",\"age\":25}"
```

---

### 🔍 Métodos Utilitários

#### **hasHeader()**
```cpp
bool hasHeader(const std::string& key) const {
    return this->headers.find(key) != this->headers.end();
}
```

**Uso:**
```cpp
if (request.hasHeader("Content-Length")) {
    // Processar body
}
```

#### **getHeader()**
```cpp
std::string getHeader(const std::string& key) const {
    std::map<std::string, std::string>::const_iterator it = headers.find(key);
    if (it != headers.end())
        return it->second;
    return "";
}
```

**Uso:**
```cpp
std::string host = request.getHeader("Host");
std::string contentType = request.getHeader("Content-Type");
```

#### **getContentLength()**
```cpp
size_t getContentLength() const {
    std::string content_length = getHeader("Content-Length");
    if (content_length.empty())
        return 0;
    return atoi(content_length.c_str());
}
```

**Uso:**
```cpp
size_t body_size = request.getContentLength();
if (body_size > MAX_BODY_SIZE)
    return error413();
```

---

## 3. ConfigHelper - Funções Auxiliares

### 📁 Arquivos
- **Header:** `include/ConfigHelper.hpp`
- **Implementação:** `src/config/ConfigHelper.cpp`

### 🎯 Propósito

Namespace com **funções especializadas** para parse de diretivas específicas.

### 🛠️ Funções Implementadas

#### **parseRoot()**
```cpp
void parseRoot(const std::string& value, std::string& root);
```
Extrai diretório raiz.
```cpp
Input:  "www"
Output: root = "www"
```

#### **parseIndex()**
```cpp
void parseIndex(const std::string& value, std::vector<std::string>& index_files);
```
Extrai múltiplos arquivos índice.
```cpp
Input:  "index.html index.htm default.html"
Output: index_files = ["index.html", "index.htm", "default.html"]
```

#### **parseClientMaxBodySize()**
```cpp
void parseClientMaxBodySize(const std::string& value, size_t& max_body_size);
```
Converte tamanho com sufixo (K, M, G).
```cpp
Input:  "10M"
Output: max_body_size = 10485760  (10 * 1024 * 1024)

Input:  "512K"
Output: max_body_size = 524288  (512 * 1024)

Input:  "1000"
Output: max_body_size = 1000
```

#### **parseErrorPage()**
```cpp
void parseErrorPage(const std::string& value, 
                    std::map<std::string, std::string>& error_pages);
```
Extrai código de erro e caminho.
```cpp
Input:  "404 /errors/404.html"
Output: error_pages["404"] = "/errors/404.html"
```

#### **parseAutoIndex()**
```cpp
void parseAutoIndex(const std::string& value, bool& autoindex);
```
Converte "on"/"off" para boolean.
```cpp
Input:  "on"
Output: autoindex = true

Input:  "off"
Output: autoindex = false
```

#### **parseRedirect()**
```cpp
void parseRedirect(const std::string& value, int& code, std::string& url);
```
Extrai código e URL de redirecionamento.
```cpp
Input:  "301 /new-location"
Output: code = 301, url = "/new-location"
```

#### **extractKeyValue()**
```cpp
void extractKeyValue(const std::string& line, 
                     std::string& key, std::string& value);
```
Separa linha em chave e valor.
```cpp
Input:  "listen 8080"
Output: key = "listen", value = "8080"

Input:  "root /var/www/html"
Output: key = "root", value = "/var/www/html"
```

#### **parseCommonConfig()** (2 overloads)
```cpp
// Para ServerConfig
void parseCommonConfig(const std::string& key, const std::string& value, 
                       ServerConfig& server);

// Para LocationConfig
void parseCommonConfig(const std::string& key, const std::string& value, 
                       LocationConfig& location);
```

Diretivas comuns a server e location:
- `root`
- `index`
- `client_max_body_size`

---

## 4. StringUtils - Utilidades

### 📁 Arquivos
- **Header:** `include/StringUtils.hpp`
- **Implementação:** `src/utils/StringUtils.cpp`

### 🎯 Propósito

Classe utilitária com **operações de manipulação de strings**.

### 🛠️ Métodos

#### **trim()**
```cpp
static std::string trim(const std::string& str);
```

Remove espaços, tabs e newlines do início e fim.

**Algoritmo:**
```cpp
std::string trim(const std::string& str) {
    // Encontrar primeiro caractere não-branco
    size_t first = str.find_first_not_of(" \t\r\n");
    
    // Encontrar último caractere não-branco
    size_t last = str.find_last_not_of(" \t\r\n");
    
    // String vazia ou só espaços
    if (first == std::string::npos || last == std::string::npos)
        return "";
    
    // Retornar substring
    return str.substr(first, last - first + 1);
}
```

**Exemplos:**
```cpp
trim("  hello  ")      → "hello"
trim("\t\n  test\r\n") → "test"
trim("   ")            → ""
trim("no spaces")      → "no spaces"
```

---

#### **parseSize()**
```cpp
static size_t parseSize(std::string size_str);
```

Converte string de tamanho para bytes, suportando sufixos K/M.

**Algoritmo:**
```cpp
size_t parseSize(std::string size_str) {
    size_t multiplier = 1;
    
    size_str = trim(size_str);
    if (size_str.empty())
        return 0;
    
    // Verificar sufixo
    char suffix = size_str[size_str.size() - 1];
    
    if (suffix == 'M' || suffix == 'm') {
        multiplier = 1024 * 1024;         // 1 MB
        size_str.erase(size_str.size() - 1);
    }
    else if (suffix == 'K' || suffix == 'k') {
        multiplier = 1024;                // 1 KB
        size_str.erase(size_str.size() - 1);
    }
    
    return std::atol(size_str.c_str()) * multiplier;
}
```

**Tabela de Conversão:**

| Input | Bytes | Explicação |
|-------|-------|------------|
| `"1024"` | 1024 | Sem sufixo = bytes |
| `"1K"` | 1024 | 1 * 1024 |
| `"10K"` | 10240 | 10 * 1024 |
| `"1M"` | 1048576 | 1 * 1024 * 1024 |
| `"10M"` | 10485760 | 10 * 1024 * 1024 |
| `"512k"` | 524288 | Case-insensitive |

---

## 5. Comparação e Boas Práticas

### 📊 Comparação dos Parsers

| Aspecto | ConfigParser | HttpRequest |
|---------|--------------|-------------|
| **Input** | Arquivo .conf | String raw HTTP |
| **Formato** | Nginx-like | RFC 7230 (HTTP/1.1) |
| **Estrutura** | Hierárquica (server/location) | Linear (linha/headers/body) |
| **Validação** | Mínima | Protocolo rígido |
| **Erros** | Runtime exception | Silent fail (valores default) |
| **Uso** | Startup (uma vez) | Por requisição |
| **Performance** | Não crítica | Crítica |

### ✅ Boas Práticas Implementadas

#### **1. Separação de Responsabilidades**
```cpp
// Parse separado da lógica
HttpRequest request = HttpRequest::parse(raw);  // Parse
Response response(request, config);             // Processamento
```

#### **2. Imutabilidade**
```cpp
// Getters const
const std::string& getMethod() const;
const std::string& getUri() const;
```

#### **3. Factory Pattern**
```cpp
// Método estático parse
static HttpRequest parse(const std::string& raw_request);
```

#### **4. Validação de Input**
```cpp
// Trim antes de processar
line = StringUtils::trim(line);

// Ignorar linhas inválidas
if (line.empty() || line[0] == '#')
    continue;
```

#### **5. Tratamento de Encodings**
```cpp
// Remover \r (Windows/HTTP)
if (!line.empty() && line[line.size() - 1] == '\r')
    line.erase(line.size() - 1);
```

#### **6. Default Values**
```cpp
// Default para raiz
if (this->uri == "/")
    this->uri = "/index.html";

// Default para header inexistente
if (content_length.empty())
    return 0;
```

#### **7. Estruturas de Dados Adequadas**
```cpp
std::map<std::string, std::string> headers;  // Lookup O(log n)
std::vector<LocationConfig> locations;        // Lista ordenada
```

---

### 🐛 Limitações Conhecidas

#### **ConfigParser**
- ❌ Não valida sintaxe completamente
- ❌ Não suporta includes
- ❌ Não valida valores (ex: porta válida)
- ❌ Mensagens de erro genéricas

#### **HttpRequest**
- ❌ Não valida URI (caracteres inválidos)
- ❌ Não suporta chunked encoding
- ❌ Body é carregado todo na memória
- ❌ Não valida Content-Length vs body real

---

### 🚀 Possíveis Melhorias

#### **ConfigParser**
```cpp
// 1. Validação robusta
void validatePort(int port) {
    if (port < 1 || port > 65535)
        throw std::runtime_error("Invalid port");
}

// 2. Mensagens de erro com linha
throw std::runtime_error("Syntax error at line " + std::to_string(line_num));

// 3. Suporte a includes
if (key == "include")
    parseIncludedFile(value);
```

#### **HttpRequest**
```cpp
// 1. Validação de URI
bool isValidUri(const std::string& uri) {
    // Verificar caracteres permitidos por RFC 3986
}

// 2. Chunked encoding
if (getHeader("Transfer-Encoding") == "chunked")
    parseChunkedBody();

// 3. Streaming de body
void appendBodyChunk(const char* data, size_t len);
```

---

### 📈 Performance

#### **ConfigParser**
- **Complexidade:** O(n) onde n = linhas do arquivo
- **Otimização:** Parse apenas na inicialização
- **Memória:** Estruturas mantidas em memória (pequenas)

#### **HttpRequest**
- **Complexidade:** O(n) onde n = tamanho da requisição
- **Otimização:** Parse incremental possível
- **Memória:** Body completo em memória (limitado por client_max_body_size)

#### **Benchmarks Estimados**

| Operação | Tempo | Observação |
|----------|-------|------------|
| Parse config (100 linhas) | ~1ms | Uma vez no startup |
| Parse HTTP GET | ~10μs | Por requisição |
| Parse HTTP POST (1MB) | ~5ms | Dominado por I/O |

---

### 🔍 Debugging Tips

#### **ConfigParser**
```cpp
// Log de parsing
std::cout << "[CONFIG] Server " << i << ": port=" << server.port << std::endl;
std::cout << "[CONFIG] Location: " << location.path << std::endl;
```

#### **HttpRequest**
```cpp
// Log de requisição
std::cout << "[REQUEST] " << method << " " << uri << " " << version << std::endl;

// Dump de headers
for (std::map<std::string, std::string>::iterator it = headers.begin(); 
     it != headers.end(); ++it) {
    std::cout << "  " << it->first << ": " << it->second << std::endl;
}
```

---

## 📚 Referências

### **ConfigParser**
- **Nginx Configuration** - nginx.org/en/docs/
- **ABNF Notation** - RFC 5234

### **HttpRequest**
- **RFC 7230** - HTTP/1.1 Message Syntax and Routing
- **RFC 7231** - HTTP/1.1 Semantics and Content
- **RFC 3986** - URI Generic Syntax

### **C++ Parsing**
- **std::istringstream** - cppreference.com
- **std::getline** - cppreference.com
- **String Operations** - cplusplus.com/reference/string/

---

## 🎓 Conceitos Aprendidos

### **1. Parsing Hierárquico**
```
server {           ← Nível 1
    location {     ← Nível 2
        ...        ← Nível 3
    }
}
```

### **2. Tokenização**
```
"listen 8080;" → ["listen", "8080"]
```

### **3. State Machine**
```
READING_SERVER → READING_LOCATION → BACK_TO_SERVER
```

### **4. Delimitadores**
```
HTTP: \r\n (fim de linha), \r\n\r\n (fim de headers)
Config: ; (fim de diretiva), { } (blocos)
```

### **5. Error Recovery**
```cpp
// Continuar em caso de erro
if (!parseLine(line))
    continue;  // Pular linha inválida
```

---

## 👨‍💻 Autor e Histórico

**nmatondo**  
- **Criação:** 19 de Fevereiro de 2026
- **Última atualização:** 2 de Março de 2026 (Integração ConfigValidator)

---

## 🏁 Conclusão

O sistema de parsing do webserver evolui para:

### ✅ **Versão 1.0 (Atual)**
- ✅ **Robusto** - ConfigValidator garante dados válidos
- ✅ **Fail-Fast** - Exceptions no startup (não em runtime)
- ✅ **Modular** - ConfigValidator separado de ConfigParser
- ✅ **Extensível** - Fácil adicionar novas validações
- ✅ **Eficiente** - Complexidade linear O(n)
- ✅ **Legível** - Código bem estruturado com namespaces
- ✅ **Standard Compliant** - HttpRequest segue RFC 7230

### 🆕 Adições (02/03/2026)
1. **ConfigValidator** - Validação robusta durante parse
2. **Interface + Port** - Binding a interface específica (ex: 127.0.0.1:8080)
3. **upload_dir** - Diretório de upload por location
4. **Detail namespace** - Funções internas em ConfigHelper
5. **Exception handling** - Fail-fast com mensagens claras
6. **Validação de métodos** - Apenas GET/POST/DELETE permitidos

### 📊 Fluxo de Funcionamento
```
Arquivo Config → ConfigParser → ConfigValidator → ServerConfig ✅
                                                  ↓ (throw se inválido)
                                                Exception

Requisição HTTP → HttpRequest::parse() → Response ✅
```

Os dois parsers trabalham juntos transformando **texto em estruturas C++** que o servidor processa com segurança e eficiência.
