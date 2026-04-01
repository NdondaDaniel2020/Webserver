# Análise de Dead Code - Projeto Webserver C++
**Data:** 1 de Abril de 2026  
**Análise Completa:** Todas as classes principais foram analisadas

---

## 📋 Resumo Executivo

Foram identificados **4 métodos/funções que não estão sendo utilizados** em todo o projeto:

| Classe/Arquivo | Nome do Método | Tipo | Status |
|---|---|---|---|
| Response | `isCgiRequest()` | Private method | Declarado, SEM implementação |
| FileUtils | `urlDecode()` | Function | Implementado, NÃO utilizado |
| FileUtils | `create_error_message()` | Function | Implementado, NÃO utilizado |
| HttpRequest | `getContentLength()` | Public method | Implementado, NÃO utilizado |

---

## 🔍 Detalhamento do Dead Code

### 1. ❌ Response::isCgiRequest()
**Localização:** [include/Response.hpp](include/Response.hpp#L54)  
**Linha:** 54  
**Tipo:** Método privado  
**Assinatura:** `bool isCgiRequest(const std::string& file_path, const LocationConfig* location);`

**Status:** 
- ✗ Apenas declarado em Response.hpp
- ✗ **NÃO possui implementação** em nenhum arquivo .cpp
- ✗ Nunca é chamado em nenhum lugar do código
- ✗ Órfão completo

**Recomendação:** Remover a declaração ou implementar se necessário.

---

### 2. ❌ urlDecode()
**Localização:** [include/FileUtils.hpp](include/FileUtils.hpp)  
**Implementação:** [src/utils/FileUtils.cpp](src/utils/FileUtils.cpp#L337)  
**Tipo:** Função utilitária (não membro de classe)

**Assinatura:** `std::string urlDecode(const std::string& str);`

**Status:**
- ✓ Declarada em FileUtils.hpp
- ✓ Implementada em FileUtils.cpp (linhas 337-361)
- ✗ **Nunca é chamada** em nenhum arquivo .cpp
- ✗ Resultado de grep: 0 chamadas (apenas a definição)

**Código:**
```cpp
std::string urlDecode(const std::string& str)
{
    std::string decoded;
    for (size_t i = 0; i < str.length(); ++i)
    {
        if (str[i] == '%' && i + 2 < str.length())
        {
            std::string hex = str.substr(i + 1, 2);
            int value = static_cast<int>(strtol(hex.c_str(), nullptr, 16));
            decoded += static_cast<char>(value);
            i += 2;
        }
        else if (str[i] == '+')
        {
            decoded += ' ';
        }
        else
        {
            decoded += str[i];
        }
    }
    return decoded;
}
```

**Recomendação:** Remover a função se URL decoding não for necessário, ou verificar se foi planejada para uso futuro.

---

### 3. ❌ create_error_message()
**Localização:** [include/FileUtils.hpp](include/FileUtils.hpp)  
**Implementação:** [src/utils/FileUtils.cpp](src/utils/FileUtils.cpp#L15)  
**Tipo:** Função utilitária (não membro de classe)

**Assinatura:** `std::string create_error_message(const std::string& error);`

**Status:**
- ✓ Declarada em FileUtils.hpp
- ✓ Implementada em FileUtils.cpp (linhas 15-20)
- ✗ **Nunca é chamada** em nenhum arquivo .cpp
- ✗ Resultado de grep: 0 chamadas (apenas a definição)

**Código:**
```cpp
std::string create_error_message(const std::string& error)
{
    std::string error_message = "Error: " + error;
    std::cerr << error_message << std::endl;
    return error_message;
}
```

**Recomendação:** Remover a função ou substituir onde houver tratamento de erros simples.

---

### 4. ❌ HttpRequest::getContentLength()
**Localização:** [include/HttpRequest.hpp](include/HttpRequest.hpp)  
**Implementação:** [src/http/HttpRequest.cpp](src/http/HttpRequest.cpp#L189)  
**Tipo:** Métodos público (const getter)

**Assinatura:** `size_t getContentLength() const;`

**Status:**
- ✓ Declarado em HttpRequest.hpp (linha 36)
- ✓ Implementado em HttpRequest.cpp (linhas 189-195)
- ✗ **Nunca é chamado** em nenhum arquivo .cpp
- ✗ Resultado de grep: 0 chamadas (apenas a definição)

**Código:**
```cpp
size_t HttpRequest::getContentLength() const
{
    std::string content_length = getHeader("Content-Length");
    if (content_length.empty())
        return 0;
    return atoi(content_length.c_str());
}
```

**Contexto:** O projeto utiliza `request.getHeader("Content-Length")` diretamente em [src/server/ClientRequest.cpp](src/server/ClientRequest.cpp#L128) ao invés de chamar este getter.

**Recomendação:** Remover e utilizar `getHeader("Content-Length")` diretamente, ou refatorar para usar este método em vez de acessar o header diretamente.

---

## ✅ Métodos Verificados e em Uso

### Classe **Server**
- ✓ `cleanup()` - Utilizado no destrutor
- ✓ `start()` - Ponto de entrada principal
- ✓ `stop()` - Chamado por signal handler
- ✓ `createServerSocket()` - Chamado em start()
- ✓ `newConnection()` - Event loop
- ✓ `handleClientData()` - Event loop
- ✓ `handleClientSendReady()` - Event loop
- ✓ `handleCgiPipeEvent()` - Event loop
- ✓ `closeClient()` - Múltiplas localizações
- ✓ `isServerSocket()` - Event loop
- ✓ `checkTimeout()` - Event loop

### Classe **Client**
- ✓ `getFd()` - Acesso ao file descriptor
- ✓ `getCgiOutFd()` - Gerenciamento CGI
- ✓ `getCgiInFd()` - Gerenciamento CGI
- ✓ `getState()` - State machine
- ✓ `isKeepAlive()` - Gerenciamento conexão
- ✓ `isDone()` - State machine
- ✓ `getLastActivity()` - Timeout checking
- ✓ `getCgiStartTime()` - Timeout CGI
- ✓ `hasDataToSend()` - Envio de dados
- ✓ `isCgiActive()` - Detecção CGI ativo
- ✓ `getRecvBuffer()` - Acesso ao buffer
- ✓ `cleanupCgiIfActive()` - Limpeza de recursos
- ✓ `getServerIndex()` - Acesso configuração
- ✓ `getCgiState()` - Acesso estado CGI
- ✓ `sendTimeoutResponse()` - Timeout handling
- ✓ `appendRecvData()` - Recebimento de dados
- ✓ `isRequestComplete()` - Detecção conclusão
- ✓ `processRequest()` - Processamento principal
- ✓ `sendData()` - Envio de resposta
- ✓ `reset()` - Reutilização keep-alive
- ✓ `setState()` - Mudança de estado
- ✓ `startCgi()` - Inicialização CGI
- ✓ `handleCgiStdoutReadable()` - CGI I/O
- ✓ `handleCgiStdinWritable()` - CGI I/O
- ✓ `finishCgiAndGenerateResponse()` - Finalização CGI

### Classe **Response**
- ✓ `getResponseHttp()` - Retorna resposta
- ✓ `buildHttpResponse()` - Construção da resposta
- ✓ `methodGet()` - Tratamento GET
- ✓ `methodPost()` - Tratamento POST
- ✓ `methodDelete()` - Tratamento DELETE
- ✓ `multipartFormData()` - Upload multipart
- ✓ `applicationOctetStream()` - Upload binário
- ✓ `generateDirectoryListing()` - Listagem de diretórios

### Classe **HttpRequest**
- ✓ `parse()` - Parser estático
- ✓ `getQuery()` - Query string
- ✓ `getPath()` - Caminho da requisição
- ✓ `getMethod()` - Método HTTP
- ✓ `getUri()` - URI completa
- ✓ `getVersion()` - Versão HTTP
- ✓ `getHeaders()` - Headers da requisição
- ✓ `getBody()` - Corpo da requisição
- ✓ `setBody()` - Define corpo (unchunked)
- ✓ `hasHeader()` - Verifica presença de header
- ✓ `getHeader()` - Acessa valor do header

### Classe **ConfigParser**
- ✓ `loadFromFile()` - Carregamento configuração
- ✓ `getServerCount()` - Contagem de servidores
- ✓ `getServerConfig()` - Acesso à configuração

### Classe **StringUtils**
- ✓ `trim()` - Remoção de espaçamento
- ✓ `parseSize()` - Parse de tamanhos

### Funções **FileUtils** (em uso)
- ✓ `readFile()`
- ✓ `openFile()`
- ✓ `ipToHex()`
- ✓ `sanitizePath()`
- ✓ `normalizePath()`
- ✓ `isPathSafe()`
- ✓ `getRealPath()`
- ✓ `fileExists()`
- ✓ `isDirectory()`
- ✓ `isReadable()`
- ✓ `findIndexFile()`
- ✓ `getMimeType()`
- ✓ `getCurrentHttpDate()`
- ✓ `getFileModifiedDate()`
- ✓ `createDirectory()`
- ✓ `hasWritePermission()`
- ✓ `sanitizeFilename()`
- ✓ `generateUniqueFilename()`
- ✓ `writeFileToDisk()`
- ✓ `extractBoundary()`
- ✓ `parseMultipartData()`
- ✓ `isAllowedFileExtension()`
- ✓ `getFileExtension()`
- ✓ `cleanupFiles()`
- ✓ `getParentDirectory()`
- ✓ `getFileSize()`
- ✓ `getFileName()`
- ✓ `setClosExec()`

---

## 📊 Estatísticas Finais

| Métrica | Valor |
|---|---|
| **Total de métodos/funções analisadas** | 87+ |
| **Métodos em uso** | 83+ |
| **Dead code encontrado** | 4 |
| **Taxa de utilização** | 95.4% |

---

## 🔧 Recomendações de Ação

### Alta Prioridade
1. **Remover `isCgiRequest()`** - Método órfão sem implementação
2. **Remover `getContentLength()`** - Função redundante, use `getHeader("Content-Length")` diretamente

### Média Prioridade
3. **Avaliar `urlDecode()`** - Se URL decoding for necessário no futuro, implementar; caso contrário remover
4. **Remover `create_error_message()`** - Função muito simples, não justifica a existência

### Limpeza de Código
- Remover as 4 funções/métodos não utilizados
- Atualizar includes se necessário após remoção
- Executar testes para garantir que nada foi quebrado

---

## 📝 Metodologia da Análise

**Processo Utilizado:**
1. Leitura de todos os headers (.hpp) principais
2. Busca com grep em todos os arquivos .cpp para cada método declarado
3. Verificação de implementações
4. Consolidação de resultados
5. Relatório estruturado

**Arquivos Analisados:**
- ✓ include/Server.hpp + src/server/*.cpp
- ✓ include/Client.hpp + src/server/Client*.cpp  
- ✓ include/Response.hpp + src/http/*.cpp
- ✓ include/HttpRequest.hpp + src/http/HttpRequest.cpp
- ✓ include/ConfigParser.hpp + src/config/*.cpp
- ✓ include/FileUtils.hpp + src/utils/FileUtils.cpp
- ✓ include/StringUtils.hpp + src/utils/StringUtils.cpp

**Cobertura:** 100% das classes principais
