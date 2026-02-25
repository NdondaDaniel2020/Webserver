# 📤 ENVIAR ARQUIVO VIA CURL POST

## 1. Upload com POST (multipart/form-data)

### Upload Simples
```bash
# Envia arquivo como campo "file"
curl -X POST -F "file=@arquivo.txt" http://localhost:8080/upload

# Especificar nome do campo
curl -X POST -F "documento=@relatorio.pdf" http://localhost:8080/upload
```

### Upload com Múltiplos Arquivos
```bash
# Vários campos diferentes
curl -X POST \
  -F "foto=@imagem.jpg" \
  -F "documento=@arquivo.pdf" \
  http://localhost:8080/upload

# Múltiplos arquivos no mesmo campo
curl -X POST \
  -F "files=@arquivo1.txt" \
  -F "files=@arquivo2.txt" \
  -F "files=@arquivo3.txt" \
  http://localhost:8080/upload
```

### Upload com Dados Adicionais
```bash
# Arquivo + outros campos do formulário
curl -X POST \
  -F "file=@foto.jpg" \
  -F "nome=João Silva" \
  -F "email=joao@example.com" \
  -F "descricao=Minha foto de perfil" \
  http://localhost:8080/upload
```

### Especificar Content-Type do Arquivo
```bash
# Define tipo MIME específico
curl -X POST \
  -F "file=@dados.json;type=application/json" \
  http://localhost:8080/upload

# Para imagens
curl -X POST \
  -F "imagem=@foto.jpg;type=image/jpeg" \
  http://localhost:8080/upload
```

### Especificar Nome do Arquivo
```bash
# Muda o nome do arquivo enviado
curl -X POST \
  -F "file=@/caminho/local/arquivo.txt;filename=novo_nome.txt" \
  http://localhost:8080/upload
```


## 2. Upload com PUT (Binary)

### Enviar Arquivo Completo no Body
```bash
# Envia arquivo bruto no corpo da requisição
curl -X PUT \
  --data-binary "@arquivo.dat" \
  -H "Content-Type: application/octet-stream" \
  http://localhost:8080/upload/arquivo.dat

# Para imagens
curl -X PUT \
  --data-binary "@imagem.png" \
  -H "Content-Type: image/png" \
  http://localhost:8080/images/logo.png
```

## 3. Upload com Autenticação

### Basic Auth
```bash
curl -X POST \
  -u "usuario:senha" \
  -F "file=@arquivo.txt" \
  http://localhost:8080/upload
```

### Bearer Token
```bash
curl -X POST \
  -H "Authorization: Bearer seu_token_aqui" \
  -F "file=@arquivo.txt" \
  http://localhost:8080/upload
```

### Com Cookie
```bash
curl -X POST \
  -b "session=abc123xyz" \
  -F "file=@arquivo.txt" \
  http://localhost:8080/upload
```

## 4. Upload com Progresso

### Mostrar Barra de Progresso
```bash
# Barra de progresso simples
curl -# -X POST -F "file=@arquivo_grande.zip" http://localhost:8080/upload

# Progresso detalhado
curl --progress-bar -X POST -F "file=@arquivo.zip" http://localhost:8080/upload
```

### Salvar Saída em Arquivo
```bash
# Salva resposta do servidor
curl -X POST \
  -F "file=@documento.pdf" \
  http://localhost:8080/upload \
  -o resposta.json
```

## 5. Upload com Dados JSON + Arquivo

### Arquivo + JSON Separados
```bash
# JSON inline + arquivo
curl -X POST \
  -F "metadata={\"titulo\":\"Meu Documento\",\"autor\":\"João\"};type=application/json" \
  -F "file=@documento.pdf" \
  http://localhost:8080/upload
```

### JSON de Arquivo + Arquivo Binário
```bash
# Lê JSON de arquivo
curl -X POST \
  -F "config=@config.json;type=application/json" \
  -F "data=@dados.bin" \
  http://localhost:8080/upload
```

## 6. Exemplos para Testar com Servidor Local

### Criar Arquivo de Teste
```bash
# Criar arquivo para teste
echo "Olá, mundo!" > teste.txt
echo '{"nome": "teste", "valor": 123}' > dados.json
```

### Teste Básico
```bash
# Upload simples
curl -X POST -F "file=@teste.txt" http://localhost:8080/upload

# Ver cabeçalhos da requisição
curl -v -X POST -F "file=@teste.txt" http://localhost:8080/upload

# Ver apenas cabeçalhos de resposta
curl -I -X POST -F "file=@teste.txt" http://localhost:8080/upload
```

### Debug Completo
```bash
# Mostra requisição e resposta completas
curl -v -X POST \
  -F "file=@teste.txt" \
  -F "descricao=Arquivo de teste" \
  http://localhost:8080/upload

# Salva trace detalhado
curl --trace-ascii debug.txt -X POST \
  -F "file=@teste.txt" \
  http://localhost:8080/upload
```

## 7. Opções Úteis

```bash
# -X     : Método HTTP (POST, PUT, GET, etc.)
# -F     : Form data (multipart/form-data)
# -d     : Data (application/x-www-form-urlencoded)
# --data-binary : Dados binários
# -H     : Header customizado
# -u     : Autenticação básica
# -b     : Cookie
# -o     : Salvar output em arquivo
# -v     : Verbose (mostra headers)
# -#     : Barra de progresso
# -i     : Incluir headers na saída
# -I     : Apenas headers (HEAD)
# --trace-ascii : Debug detalhado
```

## 8. Formato da Requisição Multipart

Quando você usa `-F`, curl envia assim:

```http
POST /upload HTTP/1.1
Host: localhost:8080
Content-Type: multipart/form-data; boundary=----WebKitFormBoundary7MA4YWxkTrZu0gW

------WebKitFormBoundary7MA4YWxkTrZu0gW
Content-Disposition: form-data; name="file"; filename="teste.txt"
Content-Type: text/plain

[conteúdo do arquivo aqui]
------WebKitFormBoundary7MA4YWxkTrZu0gW
Content-Disposition: form-data; name="descricao"

Meu arquivo de teste
------WebKitFormBoundary7MA4YWxkTrZu0gW--
```

## 9. Exemplos Práticos para Webserver

### Upload para Diretório /uploads
```bash
curl -X POST \
  -F "file=@documento.pdf" \
  http://localhost:8080/uploads
```

### Com Limite de Tamanho
```bash
# Verificar se arquivo é muito grande antes de enviar
FILE="arquivo_grande.zip"
SIZE=$(wc -c < "$FILE")
MAX_SIZE=$((10 * 1024 * 1024))  # 10MB

if [ $SIZE -le $MAX_SIZE ]; then
    curl -X POST -F "file=@$FILE" http://localhost:8080/upload
else
    echo "Arquivo muito grande: ${SIZE} bytes (máximo: ${MAX_SIZE})"
fi
```

### Script de Upload Automatizado
```bash
#!/bin/bash
# upload.sh - Script para upload de múltiplos arquivos

URL="http://localhost:8080/upload"

for file in *.txt; do
    echo "Enviando $file..."
    RESPONSE=$(curl -s -X POST -F "file=@$file" $URL)
    echo "Resposta: $RESPONSE"
done
```

# Exemplos com GET

## Download de Arquivo
```bash
# Baixar arquivo e salvar com mesmo nome do servidor
curl -X GET -O http://localhost:8080/uploads/documento.pdf

# Baixar e salvar com nome customizado
curl -X GET http://localhost:8080/uploads/documento.pdf -o meu_doc.pdf
```

## Listar Recursos
```bash
# Listar uploads (se endpoint retornar JSON/HTML)
curl -X GET http://localhost:8080/uploads

# Com query params
curl -X GET "http://localhost:8080/uploads?pagina=1&limite=20"
```

## GET com Headers
```bash
# Enviar header Authorization
curl -X GET \
  -H "Authorization: Bearer seu_token_aqui" \
  http://localhost:8080/uploads
```

# Exemplos com DELETE

## Deletar Arquivo Específico
```bash
# Remove um arquivo pelo caminho
curl -X DELETE http://localhost:8080/uploads/documento.pdf
```

## DELETE com Autenticação
```bash
# Basic Auth
curl -X DELETE \
  -u "usuario:senha" \
  http://localhost:8080/uploads/documento.pdf

# Bearer Token
curl -X DELETE \
  -H "Authorization: Bearer seu_token_aqui" \
  http://localhost:8080/uploads/documento.pdf
```

## DELETE com Query String
```bash
# Exemplo: endpoint que apaga por parâmetro
curl -X DELETE "http://localhost:8080/uploads?file=documento.pdf"
```

# Exemplos com CGI (GET, POST, DELETE)

> Exemplos assumindo scripts em `/cgi-bin/` (ajuste conforme sua rota).

## GET em CGI

```bash
# GET simples
curl -X GET "http://localhost:8080/cgi-bin/echo.py"

# GET com query string
curl -X GET "http://localhost:8080/cgi-bin/echo.py?nome=natanael&lang=pt"

# GET com header customizado
curl -X GET \
  -H "X-Request-Id: 12345" \
  "http://localhost:8080/cgi-bin/echo.py?debug=1"
```

## POST em CGI

```bash
# POST form-urlencoded
curl -X POST \
  -H "Content-Type: application/x-www-form-urlencoded" \
  -d "nome=natanael&email=n@example.com" \
  http://localhost:8080/cgi-bin/form.py

# POST JSON
curl -X POST \
  -H "Content-Type: application/json" \
  -d '{"acao":"criar","arquivo":"teste.txt"}' \
  http://localhost:8080/cgi-bin/api.py

# POST multipart (arquivo + campo)
curl -X POST \
  -F "file=@teste.txt" \
  -F "descricao=arquivo de teste" \
  http://localhost:8080/cgi-bin/upload.py
```

## DELETE em CGI

```bash
# DELETE com query param
curl -X DELETE \
  "http://localhost:8080/cgi-bin/delete.py?file=teste.txt"

# DELETE com JSON no body
curl -X DELETE \
  -H "Content-Type: application/json" \
  -d '{"file":"teste.txt"}' \
  http://localhost:8080/cgi-bin/delete.py

# DELETE com autenticação Bearer
curl -X DELETE \
  -H "Authorization: Bearer seu_token_aqui" \
  "http://localhost:8080/cgi-bin/delete.py?file=documento.pdf"
```

## Debug de CGI

```bash
# Ver request/response completos
curl -v -X GET "http://localhost:8080/cgi-bin/echo.py?teste=1"

# Ver headers da resposta CGI
curl -i -X POST \
  -H "Content-Type: application/json" \
  -d '{"ping":"pong"}' \
  http://localhost:8080/cgi-bin/api.py
```
