# � Response - Geração de Respostas HTTP

**Arquivo:** [include/Response.hpp](../include/Response.hpp) (62 linhas) | [src/http/Response.cpp](../src/http/Response.cpp) (571 linhas)

**Propósito:** Construir respostas HTTP completas (request → response) para requisições GET, POST e DELETE, com suporte a redirects, uploads de arquivos, directory listing, e validação de segurança.

**Responsabilidades Core:**
- Processar requisições HTTP e gerar respostas conformes RFC 7231
- Coordenar interação entre parsing (HttpRequest) e composição (StatusCodes)
- Validar método permitido, permissões, e path traversal
- Orquestrar upload de arquivos com validação e rollback
- Gerar directory listings HTML quando autoindex está on
- Implementar redirects configurados

**Data de Criação:** 2 de Março de 2026 (v1.0)  
**Última Atualização:** 2 de Março de 2026  
**Revisão Crítica:** 31 de Março de 2026 - Problemas de sincronização e parsing identificados  
**Revisão Final:** 1º de Abril de 2026 - GET/POST/DELETE e segurança completos  
**Autor:** nmatondo

> **👉 Navegação:** Veja [INDEX.md](INDEX.md) para índice centralizado de toda documentação

---

## 📋 Visão Geral

A classe **Response** é responsável por:

1. 🔍 **Análise de Segurança** - Validar URI (path traversal), métodos permitidos, symlinks
2. 📁 **Processamento GET** - Servir arquivos com index files, directory listing, redirects
3. 📤 **Processamento POST** - Processar uploads multipart, form data, JSON
4. 🗑️ **Processamento DELETE** - Deletar arquivos com validações rigorosas
5. 📤 **Geração de Resposta** - Construir headers HTTP + body em `response_str`

**Fluxo Típico:**
```
HttpRequest → Response constructor → buildHttpResponse() 
  → methodGet/POST/DELETE() → StatusCodes::httpXXX() 
  → response_str preenchida → Client::sendData()
```

---

## 🏗️ Estrutura da Classe

### Membros Privados

```cpp
private:
    ServerConfig config;                        // Configuração do servidor (root, index_files, etc)
    std::string response_str;                   // Resposta HTTP completa (headers + body)
    std::vector<std::string> protected_files;   // Arquivos proibidos de deletar (ex: index.html)
    std::vector<std::string> allowed_extensions;// Extensões permitidas em upload (jpg, pdf, zip, etc)
```

**Detalhamento:**

- **config** - Cópia da ServerConfig obtida durante construção. Contém:
  - `root` - Diretório raiz do servidor (www/)
  - `index_files` - Arquivos padrão para diretórios ([index.html])
  - `client_max_body_size` - Limite de tamanho de Upload (todo requisition)
  - `locations` - Array de LocationConfig para routing

- **response_str** - Buffer que acumula a resposta completa:
  - Preenchida por métodos httpXXX() de StatusCodes
  - Returned via getResponseHttp() ao Client para envio
  - Contém headers HTTP + CRLF + body

- **protected_files** - Lista de arquivos que NÃO podem ser deletados
  - Iniciada com "index.html" no construtor
  - Consultada em methodDelete() antes de remove()

- **allowed_extensions** - Whitelist de extensões permitidas em uploads
  - 11 extensões suportadas: jpg, jpeg, png, gif, pdf, txt, doc, docx, zip, mp4, mp3
  - Validação ocorre em multipartFormData() ANTES de writeFileToDisk()

### Métodos Públicos

#### Construtor

```cpp
Response::Response(const HttpRequest& request, const ServerConfig& config);
```

**Propósito:** Inicializar Response e IMEDIATAMENTE gerar resposta HTTP.

**Implementação:**
```cpp
Response(const HttpRequest& request, const ServerConfig& config) : config(config)
{
    // 1. Inicializar extensões permitidas (11 tipos)
    this->allowed_extensions.push_back(".jpg");
    this->allowed_extensions.push_back(".jpeg");
    // ... etc (jpg, jpeg, png, gif, pdf, txt, doc, docx, zip, mp4, mp3)

    // 2. Inicializar arquivos protegidos
    this->protected_files.push_back("index.html");

    // 3. 🔑 GERAR RESPOSTA (sincroniza response_str)
    buildHttpResponse(request);
}
```

**Características:**
- Construtor do lado de HttpRequest
- response_str já preenchida após construção
- Nenhuma lógica de erro: gera resposta mesmo para erros (status codes apropriados)

#### Copy Constructor & Operator=

```cpp
Response::Response(const Response &other);
Response &Response::operator=(const Response &other);
```

**Deep copy** de todos os membros:
- `response_str` - Cópia da string
- `config` - Cópia da ServerConfig (cópia rasa, valores primitivos)
- `protected_files` - Vetor copiado
- `allowed_extensions` - Vetor copiado

**Uso:** Quando Client copia Response para reutilização em keep-alive.

#### getResponseHttp()

```cpp
std::string Response::getResponseHttp();
```

**Retorna:** String completa com resposta HTTP (headers + body).

**Exemplo:**
```
HTTP/1.1 200 OK
Content-Type: text/html
Content-Length: 1234
Connection: keep-alive

<!DOCTYPE html>
<html>...
```

---

## 🔄 Métodos Privados - Orquestração

### buildHttpResponse() - Coordenador Central

```cpp
void Response::buildHttpResponse(const HttpRequest& request);
```

**Propósito:** Coordenar todo o processamento da resposta HTTP.

**Fluxo:**
```cpp
1. Sanitizar URI (path traversal prevention)
   uri = sanitizePath(request.getUri())

2. Determinar root (server default ou location override)
   root = config.root
   if (location->root.size > 0) root = location->root

3. Construir path completo
   file_path = root + removeLocationInUri(uri, location)

4. Validar método permitido para location
   if (!validateAllowedMethod(request)) → 405 Method Not Allowed

5. Dispatch para método HTTP
   if (method == "GET") methodGet(request, file_path)
   else if (method == "POST") methodPost(request)
   else if (method == "DELETE") methodDelete(request, file_path)
   else → 405 Method Not Allowed
```

**Segurança:**
- `sanitizePath()` bloqueia `..` e paths absolutos
- Location matching ANTES de processamento
- Método validado contra location's `allowed_methods`

---

## 📥 Métodos Privados - Verbos HTTP

### methodGet() - Servir Arquivos

```cpp
void Response::methodGet(const HttpRequest& request, const std::string& file_path);
```

**Propósito:** Processar requisição GET e retornar arquivo ou listing.

**Fluxo Implementado:**

```
1. Verificar redirect configurado
   if (location->redirect_code > 0) → 301/302/307/308 + Location header

2. Se é diretório:
   a) Buscar index file (index.html, etc)
      - Usar location->index_files se definido
      - Senão usar config.index_files (server default)
   
   b) Se index encontrado → servir como arquivo normal
   
   c) Se index NÃO encontrado:
      - Se autoindex ON → gerar HTML listing
      - Se autoindex OFF → 403 Forbidden

3. Se é arquivo:
   a) Verificar permissões de leitura
      if (!isReadable(file_path)) → 403 Forbidden
   
   b) Ler arquivo
      content = readFile(file_path)
   
   c) Enviar 200 OK com Content-Type apropriado
      StatusCodes::http200FileFound(response_str, request, content, file_path)

4. Se arquivo não existe → 404 Not Found
```

**Benefícios 🆕:**
- ✅ Index file override por location
- ✅ Directory listing com HTML bonito
- ✅ Validação de permissões (readable)
- ✅ RFC 7231 compliant redirect handling

---

### methodPost() - Processar Upload & Dados

```cpp
void Response::methodPost(const HttpRequest& request);
```

**Propósito:** Processar POST com múltiplos Content-Types.

#### **Fase 1: Validação de Tamanho**

```cpp
// 1. Validar client_max_body_size (server global)
if (config.client_max_body_size > 0 && body_size > config.client_max_body_size)
    → 413 Payload Too Large

// 2. Validar client_max_body_size (location específica, mais restritiva)
if (location && location->client_max_body_size > 0 && 
    body_size > location->client_max_body_size)
    → 413 Payload Too Large
```

#### **Fase 2: Obter Content-Type**

```cpp
std::string content_type = request.getHeader("Content-Type");
if (content_type.empty())
    → 400 Bad Request (RFC 7231: header obrigatório)
```

#### **Fase 3: Processar por Content-Type**

**3.1 - multipart/form-data (Upload de Arquivos)**
```cpp
if (content_type.find("multipart/form-data") != std::string::npos)
    return multipartFormData(request, content_type);
```
Chamada para multipartFormData() com processamento completo.

**3.2 - application/x-www-form-urlencoded (Form Data)**
```cpp
if (content_type.find("application/x-www-form-urlencoded") != std::string::npos)
{
    std::string decoded_body = urlDecode(request.getBody());
    // Resposta JSON simples
    return StatusCodes::http200Ok(response_str, "{\"message\":\"Form data recebido\",\"size\":...}");
}
```

**3.3 - application/json (JSON)**
```cpp
if (content_type.find("application/json") != std::string::npos)
{
    // JSON aceitado como-é (validação básica)
    return StatusCodes::http200Ok(response_str, "{\"message\":\"JSON recebido\",\"size\":...}");
}
```

**3.4 - text/plain (Texto Simples)**
```cpp
if (content_type.find("text/plain") != std::string::npos)
{
    return StatusCodes::http200Ok(response_str, "{\"message\":\"Text data recebido\",\"size\":...}");
}
```

**3.5 - Content-Type Não Suportado**
```cpp
else
    → 415 Unsupported Media Type
```

---

### methodDelete() - Deletar Arquivo

```cpp
void Response::methodDelete(const HttpRequest& request, const std::string& file_path);
```

**Propósito:** Deletar arquivo com validações rigorosas de segurança.

**Fluxo com 6 Validações:**

```cpp
// 1. Arquivo existe?
if (!fileExists(file_path)) → 404 Not Found

// 2. É diretório?
if (isDirectory(file_path)) → 403 Forbidden (não deletar diretórios)

// 3. Resolver symlinks e validar path real
std::string real_path = getRealPath(file_path);
std::string root = /* determinar root */

if (!isPathSafe(real_path, root))
    → 403 Forbidden (symlink aponta para fora do root!)

// 4. Permissão de escrita no diretório pai?
if (!hasWritePermission(getParentDirectory(file_path)))
    → 403 Forbidden

// 5. Arquivo está protegido?
if (isProtectedFile(getFileName(file_path)))
    → 403 Forbidden (ex: index.html)

// 6. Tentar deletar
if (remove(file_path.c_str()) != 0)
    → 500 Internal Server Error (erro do SO)

// Sucesso
→ 204 No Content (RFC 7231: sem body em DELETE bem-sucedido)
```

**Segurança Crítica 🆕:**
- ✅ Validação de symlinks com getRealPath()
- ✅ Verificação que path real está dentro do root permitido
- ✅ Proteção de arquivos críticos (index.html)
- ✅ Logging detalhado antes e depois de operação
- ✅ Não deletar diretórios (apenas arquivos)

---

### multipartFormData() - Orquestrador de Upload

```cpp
void Response::multipartFormData(const HttpRequest& request, const std::string& content_type);
```

**Propósito:** Processar multipart/form-data (submissão de arquivos) com validação, rollback e resposta JSON.

#### **Fase 1: Extrair Boundary**

```cpp
std::string boundary = extractBoundary(content_type);
// Content-Type: multipart/form-data; boundary=----WebKitFormBoundary
if (boundary.empty()) → 400 Bad Request
```

#### **Fase 2: Parsear Multipart Data**

```cpp
std::vector<MultipartFile> files;
if (!parseMultipartData(request.getBody(), boundary, files))
    → 415 Unsupported Media Type
```

#### **Fase 3: Determinar Upload Directory**

```cpp
std::string upload_dir = getUploadDir(request);
if (upload_dir.empty())
    → 413 Payload Too Large (body excede limite de location)
```

#### **Fase 4: Criar Diretório & Validar Permissões**

```cpp
if (!createDirectory(upload_dir)) → 500 Internal Server Error
if (!hasWritePermission(upload_dir)) → 403 Forbidden
```

#### **Fase 5: Salvar Arquivos com Validação**

Para cada arquivo no multipart:
```cpp
// 1. Validar extensão
if (!isAllowedFileExtension(files[i].filename, allowed_extensions))
    → 400 Bad Request (extensão não permitida)

// 2. Validar tamanho individual (máx 10MB por arquivo)
if (files[i].content.size() > 10 * 1024 * 1024)
    → 413 Payload Too Large

// 3. Gerar nome único (segurança + evita conflitos)
std::string unique_filename = generateUniqueFilename(files[i].filename);

// 4. Escrever para disco
if (writeFileToDisk(full_path, files[i].content))
{
    saved_files.push_back(full_path);
    // JSON response para cada arquivo bem-sucedido
    json_response << "{\"filename\":\"...\",\"size\":...}";
}
else
    // ⚠️ ROLLBACK: Limpar arquivos já salvos em erro!
    cleanupFiles(saved_files);
    → 400 Bad Request
```

#### **Fase 6: Resposta JSON 201 Created**

```json
{
  "files": [
    {
      "filename": "photo_20260302_001.jpg",
      "original_name": "photo.jpg",
      "path": "/tmp/uploads/photo_20260302_001.jpg",
      "size": 250000,
      "mime_type": "image/jpeg"
    }
  ],
  "success": true,
  "count": 1
}
```

**Características 🆕:**
- ✅ Rollback automático em erro (limpa arquivos já salvos)
- ✅ Validação de extensão (whitelist de 11 tipos)
- ✅ Nomes únicos com timestamp (previne sobrescrita)
- ✅ Limite por arquivo (10MB) + limite total (client_max_body_size)
- ✅ Resposta JSON com metadata (original_name, mime_type)
- ✅ RFC 2388 compliant (servidor pode renomear)

---

## 🔍 Métodos Privados - Utilidade

### findMatchingLocation()

```cpp
const LocationConfig* Response::findMatchingLocation(const std::string& uri) const;
```

**Propósito:** Encontrar a location mais específica que corresponda ao URI.

**Implementação:**
```cpp
// Buscar longest match (more specific location wins)
const LocationConfig* best_match = NULL;
size_t best_match_length = 0;

for (size_t i = 0; i < this->config.locations.size(); i++)
{
    const std::string& location_path = this->config.locations[i].path;
    
    // Verificar se URI começa com path da location
    if (uri.find(location_path) == 0)
    {
        // Preferir match mais longo (mais específico)
        if (location_path.size() > best_match_length)
        {
            best_match = &this->config.locations[i];
            best_match_length = location_path.size();
        }
    }
}

return best_match;  // NULL se nenhuma location corresponder
```

**Exemplo:**
```
Config:
  location / { ... }
  location /api { ... }
  location /api/users { ... }

URI /api/users/123 → Retorna /api/users/123 (longest match)
URI /api/posts → Retorna /api
URI /static → Retorna /
```

---

### validateAllowedMethod()

```cpp
bool Response::validateAllowedMethod(const HttpRequest& request);
```

**Lógica:**
```cpp
const LocationConfig* location = findMatchingLocation(request.getUri());

if (location && !location->allowed_methods.empty())
{
    // Location tem lista de métodos permitidos
    if (std::find(location->allowed_methods.begin(), 
                  location->allowed_methods.end(), 
                  request.getMethod()) == location->allowed_methods.end())
        return false;  // Método não está na lista
}

return true;  // Permitido (ou sem location config)
```

**Exemplo:**
```
Config: location /upload { allowed_methods GET POST }
GET /upload → true
POST /upload → true
DELETE /upload → false → 405 Method Not Allowed
```

---

### getUploadDir()

```cpp
std::string Response::getUploadDir(const HttpRequest& request);
```

**Lógica:**
```cpp
// 1. Validar client_max_body_size da location
const LocationConfig* location = findMatchingLocation(request.getUri());
if (location && location->client_max_body_size > 0 && 
    request.getBody().size() > location->client_max_body_size)
    return "";  // Retornar vazio = erro 413

// 2. Determinar upload_dir
std::string upload_dir = this->config.root;

if (location && !location->upload_dir.empty())
{
    if (location->upload_dir[0] != '/')  // Relativo ao root
        upload_dir = this->config.root + "/" + location->upload_dir;
    else  // Caminho absoluto
        upload_dir = location->upload_dir;
}

return upload_dir;  // Padrão: www/ ou location->upload_dir se definido
```

---

### generateDirectoryListing()

```cpp
void Response::generateDirectoryListing(const HttpRequest& request, 
                                        const std::string& dir_path, 
                                        const std::string& uri);
```

**Propósito:** Gerar HTML listing quando autoindex está ON e nenhum index file encontrado.

**HTML Gerado:**
```html
<html>
  <head><title>Index of /</title></head>
  <body>
    <h1>Index of /</h1>
    <hr>
    <ul>
      <li><a href='/index.html'>index.html</a></li>
      <li><a href='/uploads/'>uploads/</a></li>
      <li><a href='/style.css'>style.css</a></li>
    </ul>
    <hr>
  </body>
</html>
```

**Retornado com:** `StatusCodes::http200FileFound(response_str, request, html, dir_path)`

---

### handleRedirect()

```cpp
void Response::handleRedirect(int code, const std::string& url);
```

**Propósito:** Construir resposta de redirect (301/302/307/308).

**Implementação:**
```cpp
std::ostringstream oss;
oss << "HTTP/1.1 " << code;

if (code == 301) oss << " Moved Permanently\r\n";
else if (code == 302) oss << " Found\r\n";
else if (code == 307) oss << " Temporary Redirect\r\n";
else if (code == 308) oss << " Permanent Redirect\r\n";

oss << "Location: " << url << "\r\n";
oss << "Content-Length: 0\r\n";
oss << "Connection: close\r\n\r\n";

this->response_str = oss.str();
```

**Exemplo:**
```
location /old-page { return 301 /public/new-page; }

GET /old-page →
HTTP/1.1 301 Moved Permanently
Location: /public/new-page
Content-Length: 0
Connection: close
```

---

### isProtectedFile()

```cpp
bool Response::isProtectedFile(const std::string& filename);
```

**Lista de Proteção:**
```cpp
this->protected_files.push_back("index.html");
```

**Uso em DELETE:**
```cpp
if (isProtectedFile(getFileName(file_path))) → 403 Forbidden
```

---

### removeLocationInUri()

```cpp
std::string Response::removeLocationInUri(const std::string& uri, 
                                          const LocationConfig* location) const;
```

**Propósito:** Implementar "alias" behavior - remover prefixo da location do URI.

**Exemplo:**
```
Config: location /api { ... }

URI = /api/users/123
removeLocationInUri(uri, location) → /users/123

file_path = root + resultado
          = www + /users/123
          = www/users/123 (procura por usuarios, não api/usuarios)
```

**Implementação:**
```cpp
std::string uri_without_location = uri;
if (location && !location->path.empty())
{
    if (uri.find(location->path) == 0)
    {
        uri_without_location = uri.substr(location->path.length());
        if (uri_without_location.empty() || uri_without_location[0] != '/')
            uri_without_location = "/" + uri_without_location;
    }
}
return uri_without_location;
```

---

## 🔄 Ciclo de Vida de uma Requisição

### Caso 1: GET File (200 OK)

```
1. Client recebe: GET /index.html HTTP/1.1
2. Client cria: HttpRequest request(raw_data)
3. Client cria: Response response(request, config)
   
   buildHttpResponse():
   ├─ URI sanitizado: /index.html
   ├─ Location encontrada: / (raiz)
   ├─ Método validado: GET é permitido
   └─ methodGet():
       ├─ Arquivo existe? Sim
       ├─ É diretório? Não
       ├─ Readable? Sim
       ├─ Content lido: <!DOCTYPE html>...
       └─ StatusCodes::http200FileFound()
   
4. response.getHttpResponse() →
       HTTP/1.1 200 OK
       Content-Type: text/html
       Content-Length: 2341
       Connection: keep-alive
       
       <!DOCTYPE html>...

5. Client envia resposta ao socket
6. Cliente recebe resposta
```

---

### Caso 2: POST Upload (201 Created)

```
1. Cliente envia: POST /uploads HTTP/1.1
                  Content-Type: multipart/form-data; boundary=---abc123
                  
                  ---abc123
                  Content-Disposition: form-data; name="file"; filename="photo.jpg"
                  
                  [binary image data]
                  ---abc123--

2. Response response(request, config)
   
   buildHttpResponse():
   ├─ Método validado: POST permitido
   └─ methodPost():
       ├─ Body size validado contra client_max_body_size
       ├─ Content-Type: multipart/form-data
       └─ multipartFormData():
           ├─ Boundary extraído: ---abc123
           ├─ Multipart parseado: 1 arquivo (photo.jpg)
           ├─ Upload dir: www/uploads
           ├─ Extensão validada: .jpg permitido
           ├─ Arquivo renomeado: photo_20260302_001.jpg
           ├─ Escrito em: www/uploads/photo_20260302_001.jpg
           └─ StatusCodes::http201Created():
               Location: /uploads/photo_20260302_001.jpg
               
               {
                 "files": [{
                   "filename": "photo_20260302_001.jpg",
                   "original_name": "photo.jpg",
                   "size": 250000,
                   "mime_type": "image/jpeg"
                 }],
                 "success": true,
                 "count": 1
               }

3. Cliente recebe 201 + Location header com URI do arquivo
```

---

### Caso 3: DELETE File (204 No Content)

```
1. Cliente envia: DELETE /uploads/old.jpg HTTP/1.1

2. Response response(request, config)
   
   buildHttpResponse():
   ├─ Método validado: DELETE permitido
   └─ methodDelete():
       ├─ Arquivo existe? Sim
       ├─ É diretório? Não
       ├─ Symlink resolvido: /home/user/www/uploads/old.jpg
       ├─ Path dentro do root? Sim
       ├─ Permissão de escrita? Sim
       ├─ Arquivo protegido? Não
       ├─ remove() chamado
       │   Arquivo deletado!
       └─ StatusCodes::http204NoContent():
           HTTP/1.1 204 No Content
           (sem body)

3. Cliente recebe 204 (sucesso, sem body)
```

---

### Caso 4: Erro 413 (Payload Too Large)

```
1. Cliente envia: POST /api HTTP/1.1
                  Content-Length: 500000000 (500MB)
   
   buildHttpResponse():
   └─ methodPost():
       ├─ Body size > client_max_body_size (100MB)
       └─ StatusCodes::http413PayloadTooLarge():
           HTTP/1.1 413 Payload Too Large
           Content-Length: 0

2. Cliente recebe erro, conexão fechada por erro 413
```

---

## 🆕 Adições Principais (v1.0)

1. **Multipart Upload com Rollback** - Se arquivo N+1 falha, deletar N arquivos já salvos
2. **Validação de Symlinks** - DELETE verifica getRealPath() e isPathSafe()
3. **Protected Files List** - Arquivos críticos não podem ser deletados
4. **Directory Listing HTML** - Geração dinâmica quando autoindex ON
5. **Multiple Content-Type Support** - JSON, form data, plain text, multipart (não só multipart)
6. **Location-specific Redirect** - return código URL por location
7. **Copy Semantics** - Deep copy para reutilização em keep-alive

---

## 📊 Integração com Outros Componentes

```
Response
├─ Usa HttpRequest
│  ├─ getUri()
│  ├─ getMethod()
│  ├─ getHeader()
│  └─ getBody()
│
├─ Usa ServerConfig / LocationConfig
│  ├─ config.root
│  ├─ config.index_files
│  ├─ config.client_max_body_size
│  ├─ location->allowed_methods
│  ├─ location->redirect_code
│  └─ location->upload_dir
│
├─ Usa FileUtils (utilitários de arquivo)
│  ├─ sanitizePath()
│  ├─ fileExists()
│  ├─ isDirectory()
│  ├─ getRealPath()
│  ├─ isPathSafe()
│  ├─ readFile()
│  ├─ writeFileToDisk()
│  ├─ createDirectory()
│  └─ etc
│
├─ Usa StringUtils
│  ├─ urlDecode()
│  └─ extractBoundary()
│
└─ Usa StatusCodes (geração de respostas HTTP)
   ├─ http200Ok()
   ├─ http200FileFound()
   ├─ http201Created()
   ├─ http204NoContent()
   ├─ http400BadRequest()
   ├─ http403Forbidden()
   ├─ http404NotFound()
   ├─ http405MethodNotAllowed()
   ├─ http413PayloadTooLarge()
   ├─ http415UnsupportedMediaType()
   └─ http500InternalServerError()
```

---

## 🏁 Conclusão - Response v1.0

A classe **Response** é o **coração da geração de respostas HTTP**, transformando uma requisição bruta em resposta formatada e segura.

### ✅ Funcionalidades Core

- ✅ **Processamento GET** com index files e directory listing
- ✅ **Processamento POST** com suporte a 4 Content-Types
- ✅ **Processamento DELETE** com validação rigorosa de segurança
- ✅ **Upload de Arquivos** com rollback, validação de extensão e unicidade
- ✅ **Location Routing** com longest-match e override de root/index/methods
- ✅ **Redirect Support** com 301/302/307/308
- ✅ **Symlink Protection** contra path traversal
- ✅ **Copy Semantics** para keep-alive

### 🆕 Adições Significativas

1. **Multipart Rollback** - Limpeza automática em erro durante upload
2. **Symlink Validation** - getRealPath() + isPathSafe() para DELETE seguro
3. **Protected Files** - Lista de arquivos críticos que não podem ser deletados
4. **Rich Content-Type Support** - Não apenas multipart, mas JSON/form/plain também
5. **Dynamic Directory Listing** - HTML bonito quando autoindex ON
6. **Per-File Limit** - Limite de 10MB por arquivo + limite total de location
7. **Unique Filenames** - Previne sobrescrita com timestamp + random

### 📊 Arquitetura

- **Responsabilidade Única:** Gerar response_str a partir de request
- **Composição:** Usa StatusCodes para construção HTTP
- **Segurança:** Path traversal, symlinks, permissões, proteção de arquivos
- **Extensibilidade:** Suporta novos Content-Types e códigos HTTP facilmente
- **Error Handling:** Sempre retorna resposta válida, nunca cai em erro

O design garante que **qualquer requisição gera uma resposta HTTP válida**, mesmo em casos de erro, mantendo a conexão estável e segura para trabalho em keep-alive.

---

## 📚 Referências

- **RFC 7231** - HTTP/1.1 Semantics and Content
- **RFC 2388** - multipart/form-data format
- **RFC 7232** - Conditional Requests (If-Modified-Since, etc)
- **FileUtils.hpp** - Utilitários de arquivo (sanitizePath, getRealPath, etc)

---

## 👨‍💻 Autor e Histórico

**nmatondo**

- **Criação:** 2 de Março de 2026 (análise + documentação)
- **Arquivo Original:** 30 de Outubro de 2025 / Atualizado 28 de Fevereiro de 2026

---

## 📖 ANÁLISE TÉCNICA DETALHADA DOS MÉTODOS

### Estrutura da Classe Response

**Membros Privados (4):**

| Campo | Tipo | Inicialização | Descrição |
|-------|------||----|
| `config` | `ServerConfig` | Constructor | Configurações do servidor (root, index_files, limits) |
| `response_str` | `std::string` | buildHttpResponse() | Resposta HTTP completa (headers + CRLF + body) |
| `protected_files` | `std::vector<std::string>` | Constructor L120 | Arquivos não-deletáveis (padrão: "index.html") |
| `allowed_extensions` | `std::vector<std::string>` | Constructor L107-118 | Extensões permitidas em upload (11 tipos) |

**Métodos Públicos (5):**

| Método | Linhas | Assinatura |
|--------|--------|-----------|
| **Constructor** | L105-125 | `Response(const HttpRequest& req, const ServerConfig& cfg)` |
| **Destructor** | L127-128 | `~Response()` (vazio, cleanup automático) |
| **Copy Constructor** | L130-134 | `Response(const Response &other)` |
| **Operator=** | L136-145 | `Response &operator=(const Response &other)` |
| **getResponseHttp()** | L147-150 | `std::string getResponseHttp() const` |

---

### 🎯 Método: buildHttpResponse() - Orquestrador Principal

**Localização:** [src/http/Response.cpp, linhas 152-174](src/http/Response.cpp#L152-L174)

**Propósito:** Validar requisição HTTP e despachar para GET/POST/DELETE

**Fluxo de Validação (6 Fases):**

#### **Fase 1: Sanitizar URI (L154)**
```cpp
std::string uri = sanitizePath(request.getUri());
// Remove "//" → "/", controla "../" → "/"
```

#### **Fase 2: Determinar Diretório Raiz (L155-159)**
```cpp
std::string root = this->config.root;  // Padrão: /www

// Buscar location customizada que match URI
LocationConfig* location = findMatchingLocation(uri);

// Se location tem root customizado, sobrescrever
if (location && !location->root.empty()) {
    root = location->root;
}
```

#### **Fase 3: Construir Caminho Completo (L161)**
```cpp
std::string file_path = root + removeLocationInUri(uri, location);
// Exemplo: /www + /uploads/file.txt = /www/uploads/file.txt
```

#### **Fase 4: Validar Método Permitido (L163-167)**
```cpp
// Verificar se método (GET/POST/DELETE) está permitido nesta location
if (!validateAllowedMethod(request)) {
    http405MethodNotAllowed(this->response_str, request);
    return;
}
```
Retorna 405 se método não está em `location->allowed_methods`

#### **Fase 5: Despacho por Método (L169-173)**
```cpp
std::string method = request.getMethod();  // "GET", "POST", "DELETE"

if (method == "GET") {
    methodGet(request, file_path);
} else if (method == "POST") {
    methodPost(request);
} else if (method == "DELETE") {
    methodDelete(request, file_path);
} else {
    http405MethodNotAllowed(this->response_str, request);
}
```

#### **Fase 6: Retorno**
`response_str` agora contém resposta HTTP completa

**Parâmetros:**
- `request` - Requisição HTTP a processar
- Usa `this->config` e `this->response_str` implicitamente

**Chamadas Externas:** `sanitizePath()`, `findMatchingLocation()`, `removeLocationInUri()`, `validateAllowedMethod()`, `methodGet()`, `methodPost()`, `methodDelete()`

---

### 💾 Método: methodGet() - Processamento GET

**Localização:** [src/http/Response.cpp, linhas 176-213](src/http/Response.cpp#L176-L213)

**Propósito:** Servir arquivos, diretórios com index files ou directory listing

**Fluxo de 7 Fases:**

#### **Fase 1: Localizar Location (L180)**
```cpp
LocationConfig* location = findMatchingLocation(request.getUri());
```

#### **Fase 2: Verificar Handlers CGI (L181-182)**
```cpp
if (location && !location->cgi_handlers.empty()) {
    http502BadGateway(this->response_str, "Fail CGI");
    return;
}
```
⚠️ **Nota:** CGI sempre retorna 502 — método `isCgiRequest()` não implementado

#### **Fase 3: Verificar Redirects (L184-185)**
```cpp
if (location && location->redirect_code > 0) {
    handleRedirect(location->redirect_code, location->redirect_url);
    return;
}
```
Retorna 301/302/307/308 com header `Location:`

#### **Fase 4: Tratar Diretória (L186-197)**
```cpp
if (isDirectory(_file_path)) {
    // Buscar index file (index.html, etc)
    std::string index_file = findIndexFile(_file_path, 
                                           location->index_files);
    
    if (!index_file.empty()) {
        _file_path = index_file;  // Usar index encontrado
    } else if (location && location->autoindex) {
        generateDirectoryListing(_file_path, _file_path, request);
        return;
    } else {
        http403Forbidden(this->response_str, "Directory listing not allowed");
        return;
    }
}
```

❓ **Precedência Index Files:**
1. Primeiro tenta `location->index_files` (se tem custom)
2. Se não encontra e `location->autoindex=true`: gera HTML listing
3. Senão: 403 Forbidden

#### **Fase 5: Validar Existência (L198-200)**
```cpp
if (!fileExists(_file_path)) {
    http404NotFound(this->response_str, _file_path);
    return;
}
```

#### **Fase 6: Validar Permissão (L201-202)**
```cpp
if (!isReadable(_file_path)) {
    http403Forbidden(this->response_str, "Permission denied");
    return;
}
```

#### **Fase 7: Retornar Arquivo (L203-206)**
```cpp
std::string content = readFile(_file_path);
http200FileFound(this->response_str, request, content, _file_path);
// Popula headers: Content-Type, Content-Length, Last-Modified
```

**Parâmetros:**
- `request` - Requisição HTTP
- `file_path` - Caminho absoluto do arquivo/diretório

**Retorna:** Via `this->response_str` (HTTP 200/301/302/307/308/403/404/502)

---

### 📤 Método: methodPost() - Upload e Processamento

**Localização:** [src/http/Response.cpp, linhas 215-280](src/http/Response.cpp#L215-L280)

**Propósito:** Processar body HTTP (form-data, JSON, text) e uploads

**Fluxo de 8 Fases:**

#### **Fase 1: Validação Global Body Size (L217-220)**
```cpp
if (config.client_max_body_size > 0 && 
    request.getBody().length() > config.client_max_body_size) {
    http413PayloadTooLarge(this->response_str, request);
    return;
}
```
Retorna **413 Payload Too Large** se ultrapassa limite global

#### **Fase 2: Localizar Location (L222)**
```cpp
LocationConfig* location = findMatchingLocation(request.getUri());
```

#### **Fase 3: Verificar CGI (L223-224)**
```cpp
if (location && !location->cgi_handlers.empty()) {
    http502BadGateway(this->response_str, "Fail CGI");
    return;
}
```

#### **Fase 4: Validação Location Body Size (L226-229)**
```cpp
if (location && location->client_max_body_size > 0 && 
    body_size > location->client_max_body_size) {
    http413PayloadTooLarge(this->response_str, request);
    return;
}
```
⚠️ **Duplo Check:** Mesmo que Fase 1, mas location-specific

#### **Fase 5: Validar Content-Type (L231-236)**
```cpp
std::string content_type = request.getHeader("Content-Type");

if (content_type.empty()) {
    http400BadRequest(this->response_str, "Missing Content-Type");
    return;
}
```

#### **Fase 6: Despacho por Content-Type (L238-267)**

**6A: multipart/form-data - Upload de Arquivos (L238-241)**
```cpp
if (content_type.find("multipart/form-data") != npos) {
    multipartFormData(request, content_type);
    return;
}
```
Descarga para método dedicado (91 linhas)

**6B: application/x-www-form-urlencoded (L243-248)**
```cpp
else if (content_type.find("application/x-www-form-urlencoded") != npos) {
    std::string decoded = urlDecode(request.getBody());
    http200Ok(this->response_str, 
              "{\"message\": \"Form received\", \"body_size\": " + 
              std::to_string(decoded.size()) + "}");
}
```

**6C: application/json (L250-255)**
```cpp
else if (content_type.find("application/json") != npos) {
    http200Ok(this->response_str, 
              "{\"message\": \"JSON received\", \"body_size\": " + 
              std::to_string(request.getBody().size()) + "}");
}
```

**6D: text/plain (L257-262)**
```cpp
else if (content_type.find("text/plain") != npos) {
    http200Ok(this->response_str, 
              "{\"message\": \"Text received\", \"body_size\": " + 
              std::to_string(request.getBody().size()) + "}");
}
```

**6E: Content-Type Desconhecido (L263-267)**
```cpp
else {
    http415UnsupportedMediaType(this->response_str);
    return;
}
```

**Parâmetros:**
- `request` - Requisição HTTP com body

**Retorna:** 400/413/415/502 em erro, 200/201 em sucesso

---

### 🗑️ Método: methodDelete() - Segurança Crítica

**Localização:** [src/http/Response.cpp, linhas 282-338](src/http/Response.cpp#L282-L338)

**Propósito:** Deletar arquivo com múltiplas validações de segurança

**Fluxo de 10 Fases (Ordem Crítica!):**

#### **Fase 1: Verificar Existência (L284-287)**
```cpp
if (!fileExists(_file_path)) {
    http404NotFound(this->response_str, _file_path);
    return;
}
```

#### **Fase 2: Rejeitar Diretórios (L289-292)**
```cpp
if (isDirectory(_file_path)) {
    http403Forbidden(this->response_str, "Cannot delete directory");
    return;
}
```

#### **Fase 3: Resolver Symlinks (L294-296)**
```cpp
std::string real_path = getRealPath(_file_path);
if (real_path.empty()) {
    http403Forbidden(this->response_str, "Cannot resolve symlink");
    return;
}
```
⚔️ **Proteção:** Detecta e bloqueia symlinks fora boundary

#### **Fase 4: Validar Path Traversal (L301-305)**
```cpp
std::string root = (location && !location->root.empty()) ? 
                   location->root : this->config.root;

if (!isPathSafe(real_path, root)) {
    http403Forbidden(this->response_str, 
                    "Symlink outside root");
    return;
}
```
⚔️ **Proteção:** Verifica se `real_path` começa com `root/`

#### **Fase 5: Validar Permissão (L307-311)**
```cpp
std::string parent_dir = getParentDirectory(_file_path);

if (!hasWritePermission(parent_dir)) {
    http403Forbidden(this->response_str, 
                    "No write permission");
    return;
}
```
⚔️ **Proteção:** Verifica W_OK em diretório pai

#### **Fase 6: Verificar Protected Files (L313-317)**
```cpp
std::string filename = getFileName(_file_path);

if (isProtectedFile(filename)) {
    http403Forbidden(this->response_str, 
                    "File is protected");
    return;
}
```
⚔️ **Proteção:** Impede deletar `index.html` e others

#### **Fase 7: Logging (L319-322)**
```cpp
std::cout << "[DELETE] " << _file_path 
          << " (" << getFileSize(_file_path) << " bytes)" << std::endl;
```

#### **Fase 8: Deletar Arquivo (L324-327)**
```cpp
int result = remove(_file_path.c_str());

if (result < 0) {
    http500InternalServerError(this->response_str, 
                              strerror(errno));
    return;
}
```

#### **Fase 9: Sucesso (L330)**
```cpp
http204NoContent(this->response_str);
```
Retorna **204 No Content** (sem body)

#### **Fase 10: Return**

**Parâmetros:**
- `request` - Requisição HTTP
- `file_path` - Caminho absoluto arquivo a deletar

**Retorna:** 204/403/404/500

**Segurança Implementada:**
- ✅ Symlink resolution + boundary check
- ✅ Path traversal validation
- ✅ File permissions check
- ✅ Protected files whitelist
- ✅ Diretórios não podem ser deletados

---

### 📦 Método: multipartFormData() - Upload Complexo

**Localização:** [src/http/Response.cpp, linhas 340-430](src/http/Response.cpp#L340-L430)

**Propósito:** Parse upload multipart e salvar arquivos com validações

**Fluxo de 9 Fases:**

#### **Fase 1: Extrair Boundary (L342-346)**
```cpp
std::string boundary = extractBoundary(content_type);
// Busca: boundary=----WebKitFormBoundary7MA4YWxkTrZu0gW

if (boundary.empty()) {
    http415UnsupportedMediaType(this->response_str);
    return;
}
```

#### **Fase 2: Parse MIME Parts (L348-352)**
```cpp
std::vector<MultipartFile> files;

if (!parseMultipartData(request.getBody(), boundary, files)) {
    http415UnsupportedMediaType(this->response_str);
    return;
}
```
Retorna vetor de `MultipartFile` (filename, content_type, content)

#### **Fase 3: Determinar Upload Dir (L354)**
```cpp
std::string upload_dir = getUploadDir(request);
// Retorna: config.root/uploads/ ou location->upload_dir
```

#### **Fase 4: Criar Diretório (L359-363)**
```cpp
if (!createDirectory(upload_dir)) {
    http500InternalServerError(this->response_str, 
                              "Cannot create upload directory");
    return;
}
```

#### **Fase 5: Validar Permissão Escrita (L365-369)**
```cpp
if (!hasWritePermission(upload_dir)) {
    http403Forbidden(this->response_str, 
                    "No write permission on upload dir");
    return;
}
```

#### **Fase 6: Loop Validação + Salvar Arquivos (L378-411)**
```cpp
std::vector<std::string> saved_files;
int success_count = 0;

for (size_t i = 0; i < files.size(); i++) {
    // 6A: Validar Extensão (L380-384)
    if (!isAllowedFileExtension(files[i].filename, allowed_extensions)) {
        cleanupFiles(saved_files);  // Rollback
        http400BadRequest(this->response_str, 
                         "File extension not allowed");
        return;
    }
    
    // 6B: Validar Tamanho Por Arquivo (L386-391)
    if (files[i].content.size() > 10 * 1024 * 1024) {  // 10 MB
        cleanupFiles(saved_files);
        http413PayloadTooLarge(this->response_str, request);
        return;
    }
    
    // 6C: Gerar Nome Único (L393-397)
    std::string unique_filename = generateUniqueFilename(files[i].filename);
    // Resultado: "1774011860_documento.pdf"
    std::string full_path = upload_dir + "/" + unique_filename;
    
    // 6D: Escrever Arquivo em Disco (L400-407)
    if (writeFileToDisk(full_path, files[i].content)) {
        saved_files.push_back(full_path);
        success_count++;
        // JSON do arquivo adicionado ao response
    } else {
        cleanupFiles(saved_files);  // Rollback completo
        http400BadRequest(this->response_str, "Failed to write file");
        return;
    }
}
```

#### **Fase 7: Construir Response JSON (L412-423)**
```json
{
  "files": [
    {
      "filename": "1774011860_documento.pdf",
      "original_name": "documento.pdf",
      "path": "/home/.../uploads/1774011860_documento.pdf",
      "size": 524288,
      "mime_type": "application/pdf"
    }
  ],
  "success": true,
  "count": 1
}
```

#### **Fase 8: Extrair Upload URL (L414-423)**
```cpp
std::string location = "";
// Busca "/uploads/" no path do primeiro arquivo
if (full_path.find("/uploads/") != npos) {
    location = "/uploads/" + unique_filename;
} else {
    location = request.getUri() + unique_filename;
}
```

#### **Fase 9: Retornar 201 Created (L425)**
```cpp
http201Created(this->response_str, location, json_response);
// HTTP 201
// Location: /uploads/1774011860_documento.pdf
// Content-Type: application/json
// {json_response}
```

**Parâmetros:**
- `request` - Requisição com multipart body
- `content_type` - Header com boundary info

**Hardcoded Values:**
- Tamanho máx por arquivo: **10 MB** (L386)
- Timestamp para unicidade: `time(0)` + `_` + original filename

**Retorna:** 201/400/403/413/415/500

---

### 🔧 Métodos Helper Críticos

#### **Método: findMatchingLocation() - Longest Prefix Match**

**Localização:** [src/http/Response.cpp, linhas 432-449](src/http/Response.cpp#L432-L449)

```cpp
LocationConfig* findMatchingLocation(const std::string& uri)
{
    LocationConfig* best_match = NULL;
    size_t longest_prefix = 0;
    
    // Linear search em todas as locations
    for (size_t i = 0; i < this->config.locations.size(); i++) {
        std::string location_path = this->config.locations[i].path;
        
        // Prefixo match: /api/users começa com /api?
        if (uri.find(location_path) == 0) {
            // Salvar a mais longa (em caso de múltiplas matches)
            if (location_path.length() > longest_prefix) {
                best_match = &this->config.locations[i];
                longest_prefix = location_path.length();
            }
        }
    }
    
    return best_match;  // NULL se nenhuma match
}
```

**Exemplo:**
```
Locations:  /api,  /api/users,  /uploads
URI:        /api/users/john

Match /api (3 chars) → 200
Match /api/users (9 chars) → 200 (BEST, longest)
Match /uploads → 0 (não é prefixo)
Resultado: &locations[1] (api/users)
```

---

#### **Método: validateAllowedMethod() - Whitelist de Métodos**

**Localização:** [src/http/Response.cpp, linhas 451-467](src/http/Response.cpp#L451-L467)

```cpp
bool validateAllowedMethod(const HttpRequest& request)
{
    LocationConfig* location = findMatchingLocation(request.getUri());
    
    // Sem location = sem restrição (todos permitidos)
    if (!location) {
        std::string method = request.getMethod();
        return (method == "GET" || method == "POST" || method == "DELETE");
    }
    
    // Com location = verificar allowed_methods
    std::string method = request.getMethod();
    for (size_t i = 0; i < location->allowed_methods.size(); i++) {
        if (location->allowed_methods[i] == method) {
            return true;  // Permitido
        }
    }
    
    return false;  // Não em whitelist
}
```

**Exemplo:**
```
Location /api:
  allowed_methods: [GET, POST]

Request: DELETE /api/user
Resultado: false → 405 Method Not Allowed
```

---

#### **Método: generateDirectoryListing() - HTML Dinâmico**

**Localização:** [src/http/Response.cpp, linhas 493-608](src/http/Response.cpp#L493-L608)

**Propósito:** Gerar HTML com listing de arquivos e diretórios

**Estrutura (116 linhas!):**

```cpp
void generateDirectoryListing(const std::string& dir_path, 
                            const std::string& request_path,
                            const HttpRequest& request)
{
    // 1. Abrir diretório (L496-501)
    DIR* dir = opendir(dir_path.c_str());
    if (!dir) {
        http500InternalServerError(this->response_str, ...);
        return;
    }
    
    // 2. Ler entradas (L503-520)
    struct dirent* entry;
    std::vector<DirectoryEntry> entries;
    while ((entry = readdir(dir)) != NULL) {
        // Pular "." e manter ".."
        if (std::string(entry->d_name) == ".")
            continue;
        
        // Stat para tipo/tamanho/data
        struct stat file_stat;
        stat(full_path.c_str(), &file_stat);
        
        DirectoryEntry de;
        de.name = entry->d_name;
        de.is_directory = S_ISDIR(file_stat.st_mode);
        de.size = file_stat.st_size;
        de.mtime = file_stat.st_mtime;
        entries.push_back(de);
    }
    closedir(dir);
    
    // 3. Ordenar (L525-527)
    // Prioridade: ".." primeiro, depois dirs, depois arquivos, alfabético
    std::sort(entries.begin(), entries.end(), directoryEntryLess);
    
    // 4. Montar HTML (L529-605)
    std::string html = "<html><head>...";
    html += "<h1>Index of " + request_path + "</h1>";
    html += "<table>...";
    
    for (size_t i = 0; i < entries.size(); i++) {
        html += "<tr>";
        
        // Nome hyperlink
        if (entries[i].is_directory) {
            html += "<td><a href=\"" + entries[i].name + "/\">📁 " + 
                    entries[i].name + "/</a></td>";
        } else {
            html += "<td><a href=\"" + entries[i].name + "\">📄 " + 
                    entries[i].name + "</a></td>";
        }
        
        // Tamanho
        html += "<td>" + std::to_string(entries[i].size) + " bytes</td>";
        
        // Data modificação
        time_t t = entries[i].mtime;
        html += "<td>" + std::string(ctime(&t)) + "</td>";
        
        html += "</tr>";
    }
    
    html += "</table></html>";
    
    // 5. Retornar com 200 OK (L607)
    http200FileFound(this->response_str, request, html, dir_path);
}
```

**Saída Exemplo:**
```html
<html>
<h1>Index of /uploads</h1>
<table>
  <tr>
    <td><a href="../">📁 ..</a></td>
    <td>-</td>
    <td></td>
  </tr>
  <tr>
    <td><a href="image.jpg">📄 image.jpg</a></td>
    <td>2458624 bytes</td>
    <td>Mon Mar 20 10:30:00 2026</td>
  </tr>
</table>
</html>
```

---

#### **Método: handleRedirect() - Location Header**

**Localização:** [src/http/Response.cpp, linhas 610-631](src/http/Response.cpp#L610-L631)

```cpp
void handleRedirect(int code, const std::string& url)
{
    // Montar response completo
    std::string response;
    response += "HTTP/1.1 ";
    response += std::to_string(code);
    response += " ";
    
    // Código → Descrição
    if (code == 301)
        response += "Moved Permanently";
    else if (code == 302)
        response += "Found";
    else if (code == 307)
        response += "Temporary Redirect";
    else if (code == 308)
        response += "Permanent Redirect";
    
    response += "\r\n";
    response += "Location: " + url + "\r\n";
    response += "Content-Length: 0\r\n";
    response += "Connection: close\r\n";  // ⚠️ HARDCODED!
    response += "\r\n";
    
    this->response_str = response;
}
```

**Saída Exemplo (301):**
```http
HTTP/1.1 301 Moved Permanently
Location: /new-location
Content-Length: 0
Connection: close

```

⚠️ **Problema:** `Connection: close` está hardcoded, deveria ser `keep-alive` em alguns casos

---

### 🔗 INTEGRAÇÃO COM OUTRAS CLASSES

#### **HttpRequest (Métodos Chamados)**

| Método | Frequência | Contexto |
|--------|-----------|---------|
| `getUri()` | 8x | buildHttpResponse, validateAllowedMethod, findMatchingLocation, etc |
| `getMethod()` | 3x | buildHttpResponse, validateAllowedMethod |
| `getBody()` | 8x | methodPost, multipartFormData, validation |
| `getHeader(key)` | 1x | methodPost busca "Content-Type" |
| ❌ `getQuery()` | 0x | Nunca usado (gap) |
| ❌ `getPath()` | 0x | Nunca usado (gap) |
| ❌ `getVersion()` | 0x | Nunca usado (gap) |

---

#### **StatusCodes (Funções Static Chamadas)**

| Função | Casos de Uso | Linhas |
|--------|------------|--------|
| `http200FileFound()` | GET sucesso, directory listing | L206, L607 |
| `http200Ok()` | POST JSON/form/text | L248, L256, L262 |
| `http201Created()` | Upload multipart sucesso | L425 |
| `http204NoContent()` | DELETE sucesso | L330 |
| `http400BadRequest()` | POST sem Content-Type, ext inválida | L235, L381, L409 |
| `http403Forbidden()` | Sem autoindex, sem perms, DELETE protegido | L191, L202, L293+ |
| `http404NotFound()` | GET/DELETE arquivo não existe | L199, L287 |
| `http405MethodNotAllowed()` | Método não permitido | L166, L173 |
| `http413PayloadTooLarge()` | Body/arquivo ultrapassa limite | L219, L228, L357, L391 |
| `http415UnsupportedMediaType()` | Content-Type desconhecido | L236, L345, L267 |
| `http500InternalServerError()` | mkdir/remove/opendir falhou | L327, L362, L500 |
| `http502BadGateway()` | CGI handlers presentes | L182, L224 |

---

#### **ServerConfig (Campos Acessados)**

| Campo | Linhas | Contexto |
|-------|--------|---------|
| `root` | L155, L302 | Diretório padrão do servidor |
| `client_max_body_size` | L217, L226 | Limite de body para POST |
| `index_files` | L188 | Index files se dir |
| `locations` | L437 | Loop em findMatchingLocation() |

---

#### **LocationConfig (Campos Acessados)**

| Campo | Tipo | Linhas | Contexto |
|-------|------|--------|---------|
| `path` | string | L437, L647 | Prefixo da location |
| `root` | string | L158, L302 | Root customizado |
| `allowed_methods` | vector<string> | L458 | Métodos permitidos |
| `index_files` | vector<string> | L188 | Index customizado |
| `autoindex` | bool | L190 | Dir listing? |
| `cgi_handlers` | map<...> | L181, L223 | **Sempre 502 (não impl)** |
| `client_max_body_size` | size_t | L227 | Limit customizado |
| `upload_dir` | string | L484 | Dir para uploads |
| `redirect_code` | int | L184 | Código 301/302/307/308 |
| `redirect_url` | string | L185 | URL destino |

## 🎯 RECOMENDAÇÕES para Produção

### 2. Remover Hardcode de Connection: close
Ativar keep-alive em redirects para melhor performance

### 3. Integrar Error Pages
Permitir HTML customizado para 404, 403, 500 via config

### 4. Unificar Validação de Body Size
Remover duplicação, simplificar lógica

### 5. Adicionar Validação URI Length
Default 8KB, configurável, protege contra DoS

### 6. Implementar Caching Headers
ETag, Last-Modified, If-None-Match para 304 Not Modified

### 7. Adicionar HEAD Method
Idêntico a GET mas sem body (reduz largura)

### 8. Range Requests (206)
Suportar `Range: bytes=0-1023` para streams de vídeo/áudio
