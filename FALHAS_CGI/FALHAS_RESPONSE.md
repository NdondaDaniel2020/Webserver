# Análise de Falhas na Implementação do Response

## � Resumo de Status (22/03/2026)

| # | Falha | Status | Prioridade |
|---|-------|--------|-----------|
| 1 | readFile sem limite | ❌ Ainda existe | 🔴 Crítico |
| 2 | Path traversal multipart | ❌ Ainda existe | 🔴 Crítico |
| 3 | TOCTOU em methodDelete | ❌ Ainda existe | 🔴 Crítico |
| 4 | JSON injection | ❌ Ainda existe | 🔴 Crítico |
| 5 | Sem limite de arquivos | ❌ Ainda existe | 🔴 Crítico |

---

## 🔴 FALHAS CRÍTICAS

### 1. **readFile sem limite de tamanho - Memory DoS**
**Status:** ❌ **AINDA EXISTE**
**Localização:** [FileUtils.cpp](FileUtils.cpp#L22-L32)

```cpp
std::string readFile(const std::string& filepath)
{
    std::ifstream file(filepath.c_str());
    if (!file.is_open())
        return "";
    
    std::ostringstream buffer;
    buffer << file.rdbuf();  // ← Carrega arquivo inteiro na memória SEM LIMITE
    file.close();
    
    return buffer.str();
}
```

**Status Atual:**
- ❌ Nenhuma verificação de tamanho
- ❌ Nenhuma proteção contra large files
- ❌ Será chamado em GET /massive-file.iso (várias GB)
- ❌ `bad_alloc` não será tratado

**Problema:**
- `readFile()` carrega arquivo completo na memória sem limite
- `file.rdbuf()` lê tudo de uma vez
- Arquivos grandes (1GB+) esgotam memória do servidor

**Cenário de Ataque:**
```bash
# Atacante cria arquivo de 10GB
dd if=/dev/zero of=www/huge.bin bs=1M count=10240

# Request
GET /huge.bin HTTP/1.1

# Servidor tenta:
buffer << file.rdbuf()  // Aloca 10GB de RAM → bad_alloc
```

**Impacto:**
- 🔴 **DoS crítico** - Servidor pode crashar com um único request
- OOM killer mata o processo
- Sem necessidade de autenticação

**Solução Recomendada:**
Adicionar limite ao `readFile()` em [FileUtils.cpp](FileUtils.cpp#L22):
```cpp
std::string readFile(const std::string& filepath, size_t max_size = 10*1024*1024)
{
    std::ifstream file(filepath.c_str(), std::ios::binary);
    if (!file.is_open())
        return "";

    // Verificar tamanho
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    if (size > max_size)
    {
        std::cerr << "[ERROR] File too large: " << size << " bytes (max: " << max_size << ")" << std::endl;
        return "";  // Retornar vazio indica erro
    }

    std::string content;
    content.resize(size);
    file.read(&content[0], size);
    file.close();

    return content;
}
```

---

### 2. **Path traversal em multipart filename - Arbitrary file write**
**Status:** ❌ **AINDA EXISTE**
**Localização:** [FileUtils.cpp](FileUtils.cpp#L380-L400), [Response.cpp](Response.cpp#L366-L453)

**Código Atual:**
```cpp
// Em parseMultipartData (FileUtils.cpp:380-400)
// Extrair filename do Content-Disposition
size_t filename_pos = headers.find("filename=\"");
if (filename_pos != std::string::npos)
{
    filename_pos += 10;
    size_t filename_end = headers.find("\"", filename_pos);
    if (filename_end != std::string::npos)
        file.filename = headers.substr(filename_pos, filename_end - filename_pos);
        // ← filename NÃO É SANITIZADO!
}

// Em multipartFormData (Response.cpp:366-453)
for (size_t i = 0; i < files.size(); ++i)
{
    std::string unique_filename = generateUniqueFilename(files[i].filename);
    // ← Passar filename bruto para generateUniqueFilename
```

**Status Atual:**
- ❌ Nenhuma sanitização de `files[i].filename`
- ❌ `generateUniqueFilename()` não remove `../` ou `/`
- ❌ Não há validação pós-sanitização

**Ataques Possíveis:**

**Ataque 1 - Path traversal:**
```http
POST /upload HTTP/1.1
Content-Disposition: form-data; filename="../../../etc/passwd"
```
Resultado:
```
full_path = "www/uploads" + "/" + "../../../etc/passwd_12345"
         = "www/uploads/../../../etc/passwd_12345"
         = "/etc/passwd_12345"  ← Escreve fora do root!
```

**Ataque 2 - Absolute path:**
```http
Content-Disposition: form-data; filename="/var/www/html/index.html"
```
Pode sobrescrever arquivos críticos

**Impacto:**
- 🔴 **Arbitrary file write** - Escreve em qualquer lugar
- Code execution - Sobrescreve scripts PHP/Python
- Website defacement - Sobrescreve index.html

**Solução Recomendada:**
Adicionar `sanitizeFilename()` em [FileUtils.cpp](FileUtils.cpp):
```cpp
std::string sanitizeFilename(const std::string& filename)
{
    std::string safe_name;

    // 1. Extrair apenas basename (remover paths)
    size_t last_slash = filename.find_last_of("/\\");
    std::string basename = (last_slash != std::string::npos)
                          ? filename.substr(last_slash + 1)
                          : filename;

    // 2. Remover caracteres perigosos (aceitar só: a-z A-Z 0-9 . _ -)
    for (size_t i = 0; i < basename.size(); i++)
    {
        unsigned char c = basename[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-')
        {
            safe_name += c;
        }
        else
        {
            safe_name += '_';  // Substituir caracteres perigosos
        }
    }

    // 3. Validações adicionais
    if (safe_name.empty())
        safe_name = "unnamed_file";
    if (safe_name[0] == '.')
        safe_name = "_" + safe_name;  // Prevenir .htaccess
    if (safe_name.size() > 255)
        safe_name = safe_name.substr(0, 255);

    return safe_name;
}
```

Depois em Response.cpp multipartFormData:
```cpp
std::string safe_filename = sanitizeFilename(files[i].filename);
std::string unique_filename = generateUniqueFilename(safe_filename);
std::string full_path = upload_dir + "/" + unique_filename;
```

---

### 3. **Time-of-check Time-of-use (TOCTOU) em methodDelete - Race condition**
**Status:** ❌ **AINDA EXISTE**
**Localização:** [Response.cpp](Response.cpp#L296-L360)

**Código Atual:**
```cpp
void Response::methodDelete(const HttpRequest &request, const std::string &file_path)
{
    if (!fileExists(file_path))       // ← CHECK 1
        return http404NotFound(...);

    if (isDirectory(file_path))       // ← CHECK 2
        return http403Forbidden(...);

    std::string real_path = getRealPath(file_path);  // ← CHECK 3

    if (!isPathSafe(real_path, root))   // ← CHECK 4
        return http403Forbidden(...);

    if (!hasWritePermission(parent_dir)) // ← CHECK 5
        return http403Forbidden(...);

    if (isProtectedFile(filename))     // ← CHECK 6
        return http403Forbidden(...);

    // ← **WINDOW DE VULNERABILIDADE**: Arquivo pode ser substituído aqui!

    if (remove(file_path.c_str()) != 0)  // ← USE (7 verificações depois!)
        return http500InternalServerError(...);
}
```

**Status Atual:**
- ❌ 6+ verificações separadas antes de `remove()`
- ❌ Sem proteção atômica entre checks e uso
- ❌ Vulnerável a symlink races no Linux

**Ataque - Symlink race:**
```
T0:  fileExists("/uploads/victim.txt") → true
T1:  isDirectory(...) → false
T2:  getRealPath(...) → "/uploads/victim.txt"
T3:  isPathSafe(...) → true
T4:  hasWritePermission(...) → true
T5:  ← ATACANTE: rm /uploads/victim.txt && ln -s /etc/passwd /uploads/victim.txt
T6:  remove("/uploads/victim.txt") → DELETA /etc/passwd !!!
```

**Impacto:**
- 🔴 **Arbitrary file deletion** - Deletar qualquer arquivo do sistema
- Linux pode ser inoperável (/bin/bash, /sbin/init deletados)
- Privilege escalation (deletar /etc/shadow)

**Solução Recomendada:**
Usar `AT_SYMLINK_NOFOLLOW` com `unlinkat()`:
```cpp
int dir_fd = open(parent_dir.c_str(), O_RDONLY | O_DIRECTORY);
if (dir_fd < 0)
    return http403Forbidden(...);

// Verificação atômica com fstatat (não segue symlinks)
struct stat st;
if (fstatat(dir_fd, filename.c_str(), &st, AT_SYMLINK_NOFOLLOW) != 0)
{
    close(dir_fd);
    return http404NotFound(...);
}

// Rejeitar symlinks
if (S_ISLNK(st.st_mode))
{
    close(dir_fd);
    return http403Forbidden(...);
}

// Rejeitar diretórios
if (S_ISDIR(st.st_mode))
{
    close(dir_fd);
    return http403Forbidden(...);
}

// Deletar ATOMICAMENTE usando unlinkat
if (unlinkat(dir_fd, filename.c_str(), 0) != 0)
{
    close(dir_fd);
    return http500InternalServerError(...);
}

close(dir_fd);
return http204NoContent(...);
```

---

### 4. **JSON injection em respostas - XSS**
**Status:** ❌ **AINDA EXISTE**
**Localização:** [Response.cpp](Response.cpp#L266-L287), [Response.cpp](Response.cpp#L424-L428)

**Código Atual:**
```cpp
// Em multipartFormData (linha 424-428)
json_response << "{";
json_response << "\"filename\":\"" << unique_filename << "\",";
json_response << "\"original_name\":\"" << files[i].filename << "\",";  // ← SEM ESCAPE!
json_response << "\"path\":\"" << full_path << "\",";
json_response << "\"mime_type\":\"" << getMimeType(files[i].filename) << "\"";
json_response << "}";
```

**Status Atual:**
- ❌ Nenhum JSON escaping das strings
- ❌ Strings são inseridas diretamente em JSON
- ❌ Aspas, backslashes quebram JSON válido

**Ataques:**

**Ataque 1 - JSON injection:**
```
filename="test\",\"admin\":\"true"
```
Response inválido:
```json
{
  "original_name":"test","admin":"true"
}
```

**Ataque 2 - XSS via JSON:**
```
filename="</script><script>alert(document.cookie)</script>"
```
Se página renderiza JSON em `<script>` tag:
```html
<script>
  let response = {"original_name":"</script><script>alert(...)</script>"};
</script>
```
XSS executa!

**Impacto:**
- 🔴 **XSS** - Roubo de cookies, sessões, CSRF
- JSON malformado quebra parsers
- Code injection

**Solução Recomendada:**
Adicionar `jsonEscape()` em [FileUtils.cpp](FileUtils.cpp):
```cpp
std::string jsonEscape(const std::string& value)
{
    std::string escaped;
    escaped.reserve(value.size() * 2);

    for (size_t i = 0; i < value.size(); ++i)
    {
        unsigned char c = value[i];
        switch (c)
        {
            case '"':  escaped += "\\\""; break;
            case '\\': escaped += "\\\\"; break;
            case '\b': escaped += "\\b";  break;
            case '\f': escaped += "\\f";  break;
            case '\n': escaped += "\\n";  break;
            case '\r': escaped += "\\r";  break;
            case '\t': escaped += "\\t";  break;
            default:
                if (c < 0x20)  // Caracteres de controle → \uXXXX
                {
                    char buf[7];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    escaped += buf;
                }
                else
                {
                    escaped += c;
                }
                break;
        }
    }

    return escaped;
}
```

Usar em Response.cpp:
```cpp
json_response << "\"original_name\":\"" << jsonEscape(files[i].filename) << "\",";
```

---

### 5. **Sem limite de número de arquivos em multipart - DOS**
**Status:** ❌ **AINDA EXISTE**
**Localização:** [FileUtils.cpp](FileUtils.cpp#L333-L415), [Response.cpp](Response.cpp#L366-L453)

**Código Atual:**
```cpp
// Em parseMultipartData (FileUtils.cpp:333-415)
std::vector<MultipartFile> files;
while (pos < body.size())  // ← Loop sem limite!
{
    // ... parser ...
    if (!file.filename.empty())
        files.push_back(file);  // ← Pode ser 100.000x
}

// Em multipartFormData (Response.cpp:366-453)
for (size_t i = 0; i < files.size(); ++i)  // ← Processa todos sem limite
{
    // Cada arquivo: write to disk, validação, etc
}
```

**Status Atual:**
- ❌ Nenhuma validação de `files.size()`
- ❌ `parseMultipartData()` não limita número de partes
- ❌ Loop processa TODOS os arquivos

**Ataque - Multipart bomb:**
Enviar request dengan 100.000 partes, cada uma com 1-10KB:
```
postman enviando:
100.000 arquivos × 10KB = 1GB total
parseMultipartData aloca tudo em RAM
Loop processa 100.000 iterações
Cada iteração: única filename, write to disk, validação
```

**Impacto:**
- 🔴 **Memory exhaustion** - 1GB+ RAM consumido
- **Disk exhaustion** - 100.000 inodes usados
- **CPU DoS** - Servidor travado processando
- **FD exhaustion** - Muitos files abertos

**Solução Recomendada:**
Adicionar limite em parseMultipartData:
```cpp
bool parseMultipartData(const std::string& body, const std::string& boundary,
                        std::vector<MultipartFile>& files)
{
    const size_t MAX_FILES = 20;  // Limite de partes
    const size_t MAX_TOTAL_SIZE = 100 * 1024 * 1024;  // 100MB total

    // ... código de parsing ...

    while (pos < body.size())
    {
        // CHECK: Limite de arquivos
        if (files.size() >= MAX_FILES)
        {
            std::cerr << "[ERROR] Too many multipart parts" << std::endl;
            return false;
        }

        // ... resto do parsing ...
    }

    return !files.empty();
}
```

Também adicionar validação em Response::multipartFormData:
```cpp
// Verificar limite de arquivos
const size_t MAX_FILES = 20;
if (files.size() > MAX_FILES)
{
    std::cerr << "[400] Too many files: " << files.size() << std::endl;
    return StatusCodes::http400BadRequest(this->response_str,
                                          "Too many files in request",
                                          this->config);
}

// Verificar tamanho total
size_t total_size = 0;
for (size_t i = 0; i < files.size(); ++i)
    total_size += files[i].content.size();

if (total_size > 100 * 1024 * 1024)  // 100MB
{
    std::cerr << "[413] Total upload too large: " << total_size << std::endl;
    return StatusCodes::http413PayloadTooLarge(this->response_str, this->config);
}
```

---

## 🛡️ CHECKLIST DE SEGURANÇA

Antes de usar em produção:

- [ ] Limite de tamanho em `readFile()`
- [ ] Sanitização de filename em `multipartFormData()`
- [ ] Proteção TOCTOU em `methodDelete()` (usar `unlinkat`)
- [ ] JSON escaping em todas as respostas
- [ ] Limite de número de arquivos e tamanho total


**Problema:**
- `files[i].filename` vem diretamente do cliente (header `filename=`)
- Não há sanitização antes de usar como nome de arquivo
- `generateUniqueFilename()` pode não remover path separators

**Ataques:**

**Ataque 1 - Path traversal:**
```http
POST /upload HTTP/1.1
Content-Type: multipart/form-data; boundary=----WebKitFormBoundary

------WebKitFormBoundary
Content-Disposition: form-data; name="file"; filename="../../../etc/passwd"
Content-Type: text/plain

malicious content
------WebKitFormBoundary--
```

Se `generateUniqueFilename("../../../etc/passwd")` não sanitiza:
```
full_path = "www/uploads" + "/" + "../../../etc/passwd_12345"
         = "www/uploads/../../../etc/passwd_12345"
         = "/etc/passwd_12345"  ← Escreve fora do root!
```

**Ataque 2 - Null byte injection (se não tratado):**
```
filename="config.php\x00.txt"
→ Pode criar config.php (dependendo de implementação C)
```

**Ataque 3 - Overwrite com nome absoluto:**
```
filename="/var/www/html/index.html"
→ Sobrescreve arquivos críticos
```

**Impacto:**
- **Arbitrary file write** - Escreve em qualquer lugar do filesystem
- **Code execution** - Sobrescreve scripts PHP/Python
- **Privilege escalation** - Modifica /etc/passwd, .ssh/authorized_keys
- **Website defacement** - Sobrescreve index.html

**Solução Recomendada:**
```cpp
// Sanitizar filename ANTES de gerar unique_filename
std::string sanitizeFilename(const std::string& filename)
{
    std::string safe_name;

    // 1. Extrair apenas basename (remover paths)
    size_t last_slash = filename.find_last_of("/\\");
    std::string basename = (last_slash != std::string::npos)
                          ? filename.substr(last_slash + 1)
                          : filename;

    // 2. Remover caracteres perigosos
    for (size_t i = 0; i < basename.size(); i++)
    {
        unsigned char c = basename[i];

        // Aceitar apenas: a-z A-Z 0-9 . _ -
        if ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '.' || c == '_' || c == '-')
        {
            safe_name += c;
        }
        else
        {
            safe_name += '_';  // Substituir caracteres perigosos
        }
    }

    // 3. Validações adicionais
    if (safe_name.empty())
        safe_name = "unnamed_file";

    if (safe_name[0] == '.')
        safe_name = "_" + safe_name;  // Prevenir arquivos ocultos

    if (safe_name.size() > 255)
        safe_name = safe_name.substr(0, 255);  // Limite de tamanho

    return safe_name;
}

// No multipartFormData:
for (size_t i = 0; i < files.size(); ++i)
{
    // SANITIZAR antes de usar
    std::string safe_filename = sanitizeFilename(files[i].filename);
    std::string unique_filename = generateUniqueFilename(safe_filename);
    std::string full_path = upload_dir + "/" + unique_filename;

    // Validação adicional de segurança
    std::string real_path = getRealPath(full_path);
    if (!isPathSafe(real_path, upload_dir))
    {
        std::cerr << "[403] Attempted path traversal: " << files[i].filename << std::endl;
        cleanupFiles(saved_files);
        return StatusCodes::http403Forbidden(this->response_str, request, "", this->config);
    }

    // ... resto do código ...
}
```

---

### 3. **Time-of-check Time-of-use (TOCTOU) em methodDelete - Race condition**
**Localização:** `src/http/Response.cpp:296-354`

```cpp
void Response::methodDelete(const HttpRequest &request, const std::string &file_path)
{
    if (!fileExists(file_path))         // ← CHECK 1
        return http404NotFound(...);

    if (isDirectory(file_path))         // ← CHECK 2
        return http403Forbidden(...);

    std::string real_path = getRealPath(file_path);  // ← CHECK 3

    if (!isPathSafe(real_path, root))   // ← CHECK 4
        return http403Forbidden(...);

    if (!hasWritePermission(parent_dir))  // ← CHECK 5
        return http403Forbidden(...);

    if (isProtectedFile(filename))      // ← CHECK 6
        return http403Forbidden(...);

    // ← WINDOW: Arquivo pode ser substituído aqui por symlink/hardlink

    if (remove(file_path.c_str()) != 0)  // ← USE
        return http500InternalServerError(...);
}
```

**Problema:**
- 6 verificações separadas antes de `remove()`
- **TOCTOU window:** Entre checks e `remove()`, atacante pode modificar filesystem
- Linux permite race attacks com `symlink()`

**Ataque - Symlink race:**
```bash
#!/bin/bash
# Terminal 1 (Atacante)
while true; do
    touch /tmp/victim_file
    rm /tmp/victim_file
    ln -s /etc/passwd /tmp/victim_file
    rm /tmp/victim_file
done

# Terminal 2 (Atacante envia requests)
while true; do
    curl -X DELETE http://server.com/uploads/victim_file
done
```

**Timeline do ataque:**
```
T0: fileExists("/uploads/victim_file") → true (arquivo normal)
T1: isDirectory("/uploads/victim_file") → false
T2: getRealPath("/uploads/victim_file") → "/uploads/victim_file"
T3: isPathSafe(...) → true
T4: ← ATACANTE: rm victim_file && ln -s /etc/passwd victim_file
T5: remove("/uploads/victim_file") → deleta /etc/passwd !!!
```

**Impacto:**
- **Arbitrary file deletion** - Deletar qualquer arquivo do sistema
- **Privilege escalation** - Deletar /etc/shadow, logs
- **DoS** - Deletar binários críticos (/bin/bash, /sbin/init)

**Solução Recomendada:**

**Opção 1 - openat + unlinkat (melhor):**
```cpp
void Response::methodDelete(const HttpRequest &request, const std::string &file_path)
{
    // Validações iniciais
    if (!fileExists(file_path))
        return http404NotFound(...);

    // Abrir diretório pai
    std::string parent_dir = getParentDirectory(file_path);
    std::string filename = getFileName(file_path);

    int dir_fd = open(parent_dir.c_str(), O_RDONLY | O_DIRECTORY);
    if (dir_fd < 0)
        return http403Forbidden(...);

    // Fazer todas as verificações usando fstatat (atômico)
    struct stat file_stat;
    if (fstatat(dir_fd, filename.c_str(), &file_stat, AT_SYMLINK_NOFOLLOW) != 0)
    {
        close(dir_fd);
        return http404NotFound(...);
    }

    // Verificar que não é symlink
    if (S_ISLNK(file_stat.st_mode))
    {
        close(dir_fd);
        std::cerr << "[403] Cannot delete symlink" << std::endl;
        return http403Forbidden(...);
    }

    // Verificar que não é diretório
    if (S_ISDIR(file_stat.st_mode))
    {
        close(dir_fd);
        return http403Forbidden(...);
    }

    // Deletar usando unlinkat (atômico)
    if (unlinkat(dir_fd, filename.c_str(), 0) != 0)
    {
        close(dir_fd);
        return http500InternalServerError(...);
    }

    close(dir_fd);
    return http204NoContent(...);
}
```

**Opção 2 - Adicionar O_NOFOLLOW checks:**
```cpp
// Verificar symlink IMEDIATAMENTE antes de delete
struct stat st;
if (lstat(file_path.c_str(), &st) != 0)
    return http404NotFound(...);

if (S_ISLNK(st.st_mode))
{
    std::cerr << "[403] Cannot delete symlink" << std::endl;
    return http403Forbidden(...);
}

// Delete imediatamente (minimizar window)
if (remove(file_path.c_str()) != 0)
    return http500InternalServerError(...);
```

---

### 4. **JSON injection em respostas - XSS e data corruption**
**Localização:** `src/http/Response.cpp:266-287, 424-428`

```cpp
// Linha 266-267
json_response << "{\"message\":\"Form data recebido\",";
json_response << "\"size\":" << decoded_body.size() << "}";
// ← Se decoded_body contém aspas, quebra JSON

// Linha 424-428
json_response << "{";
json_response << "\"filename\":\"" << unique_filename << "\",";
json_response << "\"original_name\":\"" << files[i].filename << "\",";  // ← NÃO ESCAPED!
json_response << "\"path\":\"" << full_path << "\",";
json_response << "\"mime_type\":\"" << getMimeType(files[i].filename) << "\"";
```

**Problema:**
- Strings inseridas diretamente em JSON sem escape
- Aspas, backslashes, caracteres de controle quebram formato
- JSON malformado → XSS no frontend, crashes de parsers

**Ataques:**

**Ataque 1 - JSON injection:**
```http
POST /upload HTTP/1.1
Content-Type: multipart/form-data; boundary=----WebKitFormBoundary

------WebKitFormBoundary
Content-Disposition: form-data; name="file"; filename="test\",\"injected\":\"evil"
------WebKitFormBoundary--
```

Response:
```json
{
  "filename":"test_12345",
  "original_name":"test","injected":"evil",  ← JSON QUEBRADO
  "path":"www/uploads/test_12345"
}
```

**Ataque 2 - XSS via JSON:**
```
filename="</script><script>alert(document.cookie)</script>"
```

Response HTML renderizado:
```html
<script>
  let response = {"original_name":"</script><script>alert(document.cookie)</script>"};
</script>
```
→ XSS executa!

**Ataque 3 - Unicode bypass:**
```
filename="test\u0022,\u0022evil\u0022:\u0022payload"
→ Quando decodificado: test","evil":"payload
```

**Impacto:**
- **XSS** - Roubo de cookies, sessões
- **JSON corruption** - Parser crashes
- **Data injection** - Modifica estrutura da resposta

**Solução Recomendada:**
```cpp
std::string jsonEscape(const std::string& value)
{
    std::string escaped;
    escaped.reserve(value.size() * 2);  // Pré-alocar

    for (size_t i = 0; i < value.size(); ++i)
    {
        unsigned char c = value[i];

        switch (c)
        {
            case '"':  escaped += "\\\""; break;
            case '\\': escaped += "\\\\"; break;
            case '\b': escaped += "\\b";  break;
            case '\f': escaped += "\\f";  break;
            case '\n': escaped += "\\n";  break;
            case '\r': escaped += "\\r";  break;
            case '\t': escaped += "\\t";  break;
            default:
                // Caracteres de controle (0x00-0x1F) → \uXXXX
                if (c < 0x20)
                {
                    char buf[7];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    escaped += buf;
                }
                else
                {
                    escaped += c;
                }
                break;
        }
    }

    return escaped;
}

// Usar em todas as respostas JSON:
json_response << "{";
json_response << "\"filename\":\"" << jsonEscape(unique_filename) << "\",";
json_response << "\"original_name\":\"" << jsonEscape(files[i].filename) << "\",";
json_response << "\"path\":\"" << jsonEscape(full_path) << "\",";
json_response << "\"mime_type\":\"" << jsonEscape(getMimeType(files[i].filename)) << "\"";
json_response << "}";
```

---

### 5. **Sem limite de número de arquivos em multipart - Resource exhaustion**
**Localização:** `src/http/Response.cpp:366-453`

```cpp
std::vector<MultipartFile> files;
if (!parseMultipartData(request.getBody(), boundary, files))
    return http415UnsupportedMediaType(...);

// ← Sem verificação de files.size()

for (size_t i = 0; i < files.size(); ++i)  // ← Loop ilimitado
{
    // Processa cada arquivo...
}
```

**Problema:**
- `parseMultipartData()` não limita número de partes
- Atacante pode enviar milhares de arquivos em um request
- Cada arquivo consome:
  - RAM (arquivo em memória)
  - Disk I/O (write)
  - File descriptors
  - CPU (processamento)

**Ataque - Multipart bomb:**
```http
POST /upload HTTP/1.1
Content-Type: multipart/form-data; boundary=----WebKitFormBoundary
Content-Length: 50000000

------WebKitFormBoundary
Content-Disposition: form-data; name="file1"; filename="1.txt"

A
------WebKitFormBoundary
Content-Disposition: form-data; name="file2"; filename="2.txt"

A
------WebKitFormBoundary
... [repetir 100.000x]
```

**Consequências:**
1. **Memory exhaustion:**
   - 100.000 arquivos × 1KB cada = 100MB RAM
   - Mais metadata de `MultipartFile` structs

2. **Disk exhaustion:**
   - 100.000 arquivos criados no upload_dir
   - Pode esgotar inodes do filesystem

3. **CPU DoS:**
   - Loop processa 100.000 iterações
   - Cada uma: validação, unique filename, write, stat

4. **FD exhaustion:**
   - Se `writeFileToDisk` mantém files abertos

**Impacto:**
- DoS crítico
- Servidor trava processando um request
- Filesystem pode ficar inutilizável

**Solução Recomendada:**
```cpp
void Response::multipartFormData(const HttpRequest &request, const std::string &content_type)
{
    std::string boundary = extractBoundary(content_type);
    if (boundary.empty())
        return http415UnsupportedMediaType(...);

    std::vector<MultipartFile> files;
    if (!parseMultipartData(request.getBody(), boundary, files))
        return http415UnsupportedMediaType(...);

    // ADICIONAR: Limite de número de arquivos
    const size_t MAX_FILES = 20;  // Configurável
    if (files.size() > MAX_FILES)
    {
        std::cerr << "[400] Too many files: " << files.size()
                  << " (max: " << MAX_FILES << ")" << std::endl;
        return StatusCodes::http400BadRequest(this->response_str,
                                               "Too many files in request",
                                               this->config);
    }

    // Também verificar tamanho total
    size_t total_size = 0;
    for (size_t i = 0; i < files.size(); ++i)
        total_size += files[i].content.size();

    const size_t MAX_TOTAL_SIZE = 100 * 1024 * 1024;  // 100MB
    if (total_size > MAX_TOTAL_SIZE)
    {
        std::cerr << "[413] Total upload size too large: " << total_size << std::endl;
        return StatusCodes::http413PayloadTooLarge(this->response_str, this->config);
    }

    // ... resto do código ...
}
```

Também modificar `parseMultipartData` em `FileUtils.cpp`:
```cpp
bool parseMultipartData(const std::string& body, const std::string& boundary,
                        std::vector<MultipartFile>& files,
                        size_t max_parts = 20)  // ← Adicionar limite
{
    // ... código existente ...

    while (pos < body.size())
    {
        // ADICIONAR: Check de limite
        if (files.size() >= max_parts)
        {
            std::cerr << "[ERROR] Too many multipart parts" << std::endl;
            return false;
        }

        // ... resto do parsing ...
    }
}
```

---

## 🟠 FALHAS IMPORTANTES

### 6. **findMatchingLocation usa prefix match incorreto - Ambiguidade**
**Localização:** `src/http/Response.cpp:456-476`

```cpp
const LocationConfig *Response::findMatchingLocation(const std::string &uri) const
{
    const LocationConfig *best_match = NULL;
    size_t best_match_length = 0;

    for (size_t i = 0; i < this->config.locations.size(); i++)
    {
        const std::string &location_path = this->config.locations[i].path;

        if (uri.find(location_path) == 0)  // ← substring match, não path match
        {
            if (location_path.size() > best_match_length)
            {
                best_match = &this->config.locations[i];
                best_match_length = location_path.size();
            }
        }
    }

    return best_match;
}
```

**Problema:**
- `uri.find(location_path) == 0` é substring match, não path segment match
- Causa ambiguidades e matches incorretos

**Bugs:**

**Bug 1 - Partial match:**
```
Configuração:
  location /api { ... }

Requests:
  /api/users     → Match ✓ (correto)
  /api_old/data  → Match ✗ (INCORRETO!)
                   ↑ Match porque "/api_old" começa com "/api"
```

**Bug 2 - Sem trailing slash handling:**
```
Configuração:
  location /admin/ { ... }

Requests:
  /admin/panel → NO match (falta / no URI)
  /admin       → NO match (não termina com /)
```

**Impacto:**
- Rotas erradas são aplicadas
- Bypasses de restrições de acesso
- Configurações aplicadas incorretamente

**Solução Recomendada:**
```cpp
const LocationConfig *Response::findMatchingLocation(const std::string &uri) const
{
    const LocationConfig *best_match = NULL;
    size_t best_match_length = 0;

    for (size_t i = 0; i < this->config.locations.size(); i++)
    {
        const std::string &location_path = this->config.locations[i].path;

        // Match apenas se:
        // 1. URI começa com location_path E
        // 2. Próximo caractere é '/' ou fim de string
        if (uri.compare(0, location_path.size(), location_path) == 0)
        {
            size_t next_pos = location_path.size();

            // Validar boundary
            if (next_pos == uri.size() ||           // URI exato
                uri[next_pos] == '/' ||             // Próximo é /
                location_path[location_path.size()-1] == '/')  // location termina com /
            {
                if (location_path.size() > best_match_length)
                {
                    best_match = &this->config.locations[i];
                    best_match_length = location_path.size();
                }
            }
        }
    }

    return best_match;
}
```

---

### 7. **Validação duplicada de client_max_body_size - Lógica confusa**
**Localização:** `src/http/Response.cpp:231-249`

```cpp
void Response::methodPost(const HttpRequest &request)
{
    size_t body_size = request.getBody().size();

    // Validação 1: Server-level
    if (this->config.client_max_body_size > 0 && body_size > this->config.client_max_body_size)
    {
        return http413PayloadTooLarge(...);
    }

    const LocationConfig *location = findMatchingLocation(request.getUri());
    if (location && !location->cgi_handlers.empty())
        return http502BadGateway(...);

    // Validação 2: Location-level (mesma coisa!)
    if (location && location->client_max_body_size > 0 && body_size > location->client_max_body_size)
    {
        return http413PayloadTooLarge(...);
    }
}
```

**Problemas:**
1. **Validação duplicada:** Feito em Response E em Client (Client.cpp:297-309)
2. **Ordem errada:** Deveria validar location primeiro (mais restritivo)
3. **CGI check no meio:** Interrompe fluxo de validação
4. **Redundância:** Mesma validação em `getUploadDir()` (linhas 502-509)

**Impacto:**
- Código confuso e difícil de manter
- Possível bypass se ordem mudar
- Validação inconsistente entre métodos

**Solução Recomendada:**
```cpp
void Response::methodPost(const HttpRequest &request)
{
    // Validação já foi feita em Client::parseHeaders()
    // Apenas processar aqui

    const LocationConfig *location = findMatchingLocation(request.getUri());

    // CGI check logo no início
    if (location && !location->cgi_handlers.empty())
        return http502BadGateway(...);

    std::string content_type = request.getHeader("Content-Type");
    // ... resto do código sem validações redundantes ...
}
```

---

### 8. **Directory listing não valida symlinks - Information disclosure**
**Localização:** `src/http/Response.cpp:524-669`

```cpp
void Response::generateDirectoryListing(const HttpRequest &request, const std::string &dir_path, const std::string &uri)
{
    DIR *dir = opendir(dir_path.c_str());
    if (!dir)
        return http500InternalServerError(...);

    while ((entry = readdir(dir)) != NULL)
    {
        std::string name = entry->d_name;
        std::string full_path = dir_path + "/" + name;

        struct stat file_stat;
        if (stat(full_path.c_str(), &file_stat) == 0)  // ← Segue symlinks!
        {
            item.is_directory = S_ISDIR(file_stat.st_mode);
            item.size = file_stat.st_size;
            item.mtime = file_stat.st_mtime;
        }
        // ... adiciona ao listing ...
    }
}
```

**Problema:**
- `stat()` segue symlinks automaticamente
- Symlinks apontando fora do root são listados
- Tamanho e informações de arquivos protegidos são expostos

**Ataque:**
```bash
# Criar symlink para fora do root
cd www/public
ln -s /etc/passwd leaked_passwd
ln -s /root/.ssh/id_rsa leaked_key

# Listar diretório
GET /public/ HTTP/1.1
```

Response mostra:
```html
<a href="leaked_passwd">leaked_passwd</a>  File  1247 bytes
<a href="leaked_key">leaked_key</a>        File  3247 bytes
```

**Informações vazadas:**
- Existência de arquivos sensíveis
- Tamanhos exatos
- Datas de modificação
- Estrutura de diretórios fora do root

**Impacto:**
- Information disclosure
- Reconnaissance para ataques
- Vazamento de estrutura do sistema

**Solução Recomendada:**
```cpp
while ((entry = readdir(dir)) != NULL)
{
    std::string name = entry->d_name;
    if (name == ".")
        continue;

    std::string full_path = dir_path;
    if (!full_path.empty() && full_path[full_path.size() - 1] != '/')
        full_path += '/';
    full_path += name;

    // USAR lstat ao invés de stat (não segue symlinks)
    struct stat file_stat;
    if (lstat(full_path.c_str(), &file_stat) == 0)
    {
        // Validar symlinks
        if (S_ISLNK(file_stat.st_mode))
        {
            // Resolver symlink e validar target
            char link_target[PATH_MAX];
            ssize_t len = readlink(full_path.c_str(), link_target, sizeof(link_target)-1);
            if (len > 0)
            {
                link_target[len] = '\0';
                std::string real_target = getRealPath(link_target);

                // Verificar se target está dentro do root
                if (!isPathSafe(real_target, this->config.root))
                {
                    std::cerr << "[SECURITY] Symlink outsidde root: "
                              << name << " -> " << real_target << std::endl;
                    continue;  // Pular este entry
                }
            }
        }

        item.is_directory = S_ISDIR(file_stat.st_mode);
        item.size = file_stat.st_size;
        item.mtime = file_stat.st_mtime;
    }

    entries.push_back(item);
}
```

---

### 9. **Extensões permitidas hardcoded - Inflexível**
**Localização:** `src/http/Response.cpp:115-125`

```cpp
Response::Response(const HttpRequest &request, const ServerConfig &config) : config(config)
{
    this->allowed_extensions.push_back(".jpg");
    this->allowed_extensions.push_back(".jpeg");
    this->allowed_extensions.push_back(".png");
    // ... mais 8 extensões hardcoded
```

**Problemas:**
1. **Hardcoded:** Não pode ser configurado
2. **Limitado:** Apenas 11 extensões
3. **No constructor:** Executado a cada request
4. **Não configurável por location**

**Impacto:**
- Usuário não pode fazer upload de tipos específicos (.svg, .webp, etc)
- Necessário recompilar para adicionar tipos
- Performance ruim (reinicia vetor a cada request)

**Solução:**
Mover para configuração:
```conf
server {
    allowed_upload_extensions .jpg .png .pdf .txt .zip;
}

location /uploads {
    allowed_upload_extensions .jpg .jpeg .png .gif;
}
```

---

### 10. **opendir sem close em error paths - FD leak**
**Localização:** `src/http/Response.cpp:526-531`

```cpp
DIR *dir = opendir(dir_path.c_str());
if (!dir)
{
    StatusCodes::http500InternalServerError(...);
    return;  // ← Correto, não alocou
}

// ... loop de readdir ...

closedir(dir);  // ← linha 571
```

**Problema:**
- Se houver exception entre `opendir` e `closedir`, FD vaza
- Se `StatusCodes` ou outros códigos lançarem exception, leak

**Impacto médio:**
- Leak de FD se houver exception
- Vários requests → esgota FDs

**Solução:**
```cpp
DIR *dir = opendir(dir_path.c_str());
if (!dir)
{
    StatusCodes::http500InternalServerError(...);
    return;
}

// RAII wrapper
struct DirCloser {
    DIR *d;
    DirCloser(DIR *dir) : d(dir) {}
    ~DirCloser() { if (d) closedir(d); }
} dir_closer(dir);

// ... resto do código ...
// closedir() automático no destruidor
```

---

## 🟡 FALHAS MÉDIAS

### 11. **HTML escape incompleto em directory listing**
**Localização:** `src/http/Response.cpp:42-60`

```cpp
static std::string htmlEscape(const std::string &value)
{
    std::string escaped;
    for (size_t i = 0; i < value.size(); ++i)
    {
        if (value[i] == '&')
            escaped += "&amp;";
        else if (value[i] == '<')
            escaped += "&lt;";
        else if (value[i] == '>')
            escaped += "&gt;";
        else if (value[i] == '"')
            escaped += "&quot;";
        else
            escaped += value[i];  // ← Faltam: ' \n \r e outros
    }
    return escaped;
}
```

**Problema:**
- Não escapa `'` (aspas simples)
- Não escapa newlines/carriage returns
- Pode causar XSS em alguns contextos

**Exemplo:**
```
Filename: test';alert(1);//.txt
HTML: <a href="test';alert(1);//.txt">...
      ↑ XSS se usado em atributo com aspas simples
```

**Solução:**
```cpp
else if (value[i] == '\'')
    escaped += "&#39;";
else if (value[i] == '\n')
    escaped += "&#10;";
else if (value[i] == '\r')
    escaped += "&#13;";
```

---

### 12. **Sem Content-Security-Policy headers - XSS defense faltando**

Response não adiciona headers de segurança:
- `Content-Security-Policy`
- `X-Frame-Options`
- `X-Content-Type-Options`

**Solução:** Adicionar em todas as respostas HTML.

---

### 13. **methodDelete verbose demais - Information disclosure**

Linhas 343-345 logam demais:
```cpp
std::cout << "[DELETE] Arquivo: " << file_path << std::endl;
std::cout << "[DELETE] Tamanho: " << file_size << " bytes" << std::endl;
std::cout << "[DELETE] URI: " << request.getUri() << std::endl;
```

Expõe estrutura interna de paths no console.

---

## 📋 RECOMENDAÇÕES DE PRIORIZAÇÃO

### 🔴 URGENTE (Segurança Crítica)
1. **Limitar readFile** - DoS crítico, único request derruba servidor
2. **Sanitizar multipart filenames** - Arbitrary file write, code execution
3. **Escapar JSON** - XSS, data corruption
4. **Limitar número de arquivos multipart** - Resource exhaustion DoS
5. **Fixar TOCTOU em methodDelete** - Arbitrary file deletion

### 🟠 IMPORTANTE (Robustez)
6. **Corrigir findMatchingLocation** - Bypasses de segurança
7. **Validar symlinks em directory listing** - Information disclosure
8. **Remover validações duplicadas** - Simplificar código
9. **Handle FD leaks** - opendir/closedir RAII
10. **Extensões configuráveis** - Flexibilidade

### 🟡 MÉDIO (Qualidade)
11. **HTML escape completo** - XSS defense
12. **Security headers** - Defense-in-depth
13. **Reduzir logging verboso** - Information disclosure menor

---

## 🛡️ CHECKLIST DE SEGURANÇA

Antes de produção:

- [ ] readFile com limite de tamanho (10MB max)
- [ ] Sanitização de filename em uploads
- [ ] Escape de JSON em todas as respostas
- [ ] Limite de arquivos multipart (20 max)
- [ ] TOCTOU fix em methodDelete (openat/unlinkat)
- [ ] findMatchingLocation path-aware
- [ ] lstat ao invés de stat em listings
- [ ] Validação de symlinks
- [ ] RAII para DIR*
- [ ] Security headers (CSP, X-Frame-Options)

---

## 📚 REFERÊNCIAS

- **OWASP Path Traversal:** https://owasp.org/www-community/attacks/Path_Traversal
- **TOCTOU Attacks:** https://cwe.mitre.org/data/definitions/367.html
- **JSON Injection:** https://owasp.org/www-community/vulnerabilities/JSON_Injection
- **File Upload Security:** https://owasp.org/www-community/vulnerabilities/Unrestricted_File_Upload
- **Código relevante:**
  - `src/http/Response.cpp:1-715` - Implementação completa
  - `src/utils/FileUtils.cpp` - Funções auxiliares
  - `include/Response.hpp` - Interface
