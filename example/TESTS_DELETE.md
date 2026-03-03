# 🗑️ Testes do Método DELETE

## ✅ Implementação Completa

O método DELETE foi implementado com:
- ✅ Validação de existência (404 se não existir)
- ✅ Bloqueio de diretórios (403 se for diretório)
- ✅ Validação de permissões (403 se sem permissão de escrita)
- ✅ Deleção segura com `remove()`
- ✅ Resposta 204 No Content (sucesso)
- ✅ Resposta 500 Internal Server Error (falha)
- ✅ Logs detalhados
- ✅ Path traversal protection

---

## 🧪 Testes Manuais

### **1. Teste de DELETE bem-sucedido**

```bash
# Criar arquivo de teste
echo "Arquivo de teste" > /tmp/uploads/test.txt

# Deletar arquivo
curl -v -X DELETE http://localhost:8080/uploads/test.txt

# Esperado:
# HTTP/1.1 204 No Content
# Date: ...
# Server: webserv/1.0
# Connection: close
```

**Log do servidor:**
```
[DELETE] Arquivo: /tmp/uploads/test.txt
[DELETE] Tamanho: 18 bytes
[DELETE] URI: /uploads/test.txt
[DELETE] ✓ Arquivo deletado com sucesso
[204] No Content - Recurso deletado
```

---

### **2. Teste de arquivo não encontrado (404)**

```bash
curl -v -X DELETE http://localhost:8080/uploads/inexistente.jpg

# Esperado:
# HTTP/1.1 404 Not Found
# Content-Type: text/html
# <html><body><h1>404 Not Found</h1></body></html>
```

**Log do servidor:**
```
[404] Arquivo não encontrado para deletar: www/uploads/inexistente.jpg
```

---

### **3. Teste de deleção de diretório (403)**

```bash
curl -v -X DELETE http://localhost:8080/uploads/

# Esperado:
# HTTP/1.1 403 Forbidden
# Content-Type: text/html
# <html><body><h1>403 Forbidden</h1></body></html>
```

**Log do servidor:**
```
[403] Não é permitido deletar diretórios: /tmp/uploads/
```

---

### **4. Teste de sem permissão (403)**

```bash
# Criar arquivo sem permissão de deleção
echo "protected" > /tmp/uploads/readonly.txt
chmod 444 /tmp/uploads/readonly.txt
chmod 555 /tmp/uploads  # Diretório sem escrita

curl -v -X DELETE http://localhost:8080/uploads/readonly.txt

# Esperado:
# HTTP/1.1 403 Forbidden

# Restaurar permissões
chmod 755 /tmp/uploads
```

**Log do servidor:**
```
[403] Sem permissão para deletar: /tmp/uploads/readonly.txt
```

---

### **5. Teste de método não permitido (405)**

```bash
# Tentar DELETE em location que não permite
curl -v -X DELETE http://localhost:8080/

# Esperado:
# HTTP/1.1 405 Method Not Allowed
# (se / não tem DELETE em allowed_methods)
```

**Log do servidor:**
```
[405] Método DELETE não permitido para: /
```

---

### **6. Teste de path traversal (403)**

```bash
curl -v -X DELETE "http://localhost:8080/uploads/../../../etc/passwd"

# Esperado:
# HTTP/1.1 403 Forbidden
# (bloqueado pelo sanitizePath)
```

**Log do servidor:**
```
[403] Path traversal bloqueado: /uploads/../../../etc/passwd
```

---

### **7. Teste de erro interno (500)**

```bash
# Simular erro (ex: arquivo locked por outro processo)
# Difícil de simular manualmente, mas o código trata:

# Se remove() falhar por qualquer motivo:
# HTTP/1.1 500 Internal Server Error
# <html><body><h1>500 Internal Server Error</h1>
# <p>Failed to delete file: Permission denied</p></body></html>
```

---

## 🔧 Script de Teste Automatizado

```bash
#!/bin/bash

# Script de testes para DELETE
# Salvar como: tests/test_delete.sh

BASE_URL="http://localhost:8080"
TEST_DIR="/tmp/uploads"

echo "🧪 Iniciando testes do DELETE..."

# 1. Preparar ambiente
mkdir -p "$TEST_DIR"
echo "test content" > "$TEST_DIR/file1.txt"
echo "test content" > "$TEST_DIR/file2.txt"
echo "test content" > "$TEST_DIR/file3.txt"

# 2. Teste: DELETE bem-sucedido
echo -n "Teste 1 (DELETE bem-sucedido): "
HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" -X DELETE "$BASE_URL/uploads/file1.txt")
if [ "$HTTP_CODE" = "204" ]; then
    echo "✅ PASS"
else
    echo "❌ FAIL (esperado 204, recebeu $HTTP_CODE)"
fi

# 3. Teste: Arquivo não existe
echo -n "Teste 2 (404 Not Found): "
HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" -X DELETE "$BASE_URL/uploads/inexistente.txt")
if [ "$HTTP_CODE" = "404" ]; then
    echo "✅ PASS"
else
    echo "❌ FAIL (esperado 404, recebeu $HTTP_CODE)"
fi

# 4. Teste: Deletar diretório
echo -n "Teste 3 (403 Forbidden - diretório): "
HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" -X DELETE "$BASE_URL/uploads/")
if [ "$HTTP_CODE" = "403" ]; then
    echo "✅ PASS"
else
    echo "❌ FAIL (esperado 403, recebeu $HTTP_CODE)"
fi

# 5. Teste: Método não permitido
echo -n "Teste 4 (405 Method Not Allowed): "
HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" -X DELETE "$BASE_URL/")
if [ "$HTTP_CODE" = "405" ]; then
    echo "✅ PASS"
else
    echo "❌ FAIL (esperado 405, recebeu $HTTP_CODE)"
fi

# 6. Teste: Path traversal
echo -n "Teste 5 (403 Path Traversal): "
HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" -X DELETE "$BASE_URL/uploads/../../../etc/passwd")
if [ "$HTTP_CODE" = "403" ]; then
    echo "✅ PASS"
else
    echo "❌ FAIL (esperado 403, recebeu $HTTP_CODE)"
fi

# Limpar
rm -f "$TEST_DIR/file2.txt" "$TEST_DIR/file3.txt"

echo ""
echo "✅ Testes concluídos!"
```

---

## 📊 Comparação com outros servidores

### **Nginx:**
```bash
curl -X DELETE http://nginx-server/file.txt
# Retorna: 204 No Content (igual ao nosso)
```

### **Apache:**
```bash
curl -X DELETE http://apache-server/file.txt
# Retorna: 200 OK (diferente, mas válido)
```

### **Express.js (Node):**
```javascript
app.delete('/file', (req, res) => {
  fs.unlink(path, () => {
    res.status(204).send();  // Igual ao nosso
  });
});
```

---

## 🔒 Segurança Implementada

1. ✅ **Path traversal protection** - Bloqueia `/../`
2. ✅ **Validação de permissões** - Verifica write permission
3. ✅ **Bloqueio de diretórios** - Não permite deletar pastas
4. ✅ **Validação de allowed_methods** - 405 se não permitido
5. ✅ **Safe path** - Garante que arquivo está dentro do root

---

## 📝 Respostas HTTP Implementadas

| Código | Descrição | Quando ocorre |
|--------|-----------|---------------|
| 204 | No Content | Arquivo deletado com sucesso |
| 404 | Not Found | Arquivo não existe |
| 403 | Forbidden | Sem permissão ou tentativa de deletar diretório |
| 405 | Method Not Allowed | DELETE não permitido nesta location |
| 500 | Internal Server Error | Erro ao executar `remove()` |

---

## 🎯 Casos de uso típicos

### **API REST - Deletar recurso**
```bash
DELETE /api/users/123 HTTP/1.1
Host: api.example.com

# Resposta: 204 No Content
```

### **Gerenciador de arquivos**
```bash
DELETE /uploads/photo.jpg HTTP/1.1
Host: storage.example.com

# Resposta: 204 No Content
```

### **Limpeza de cache**
```bash
DELETE /cache/session_abc123 HTTP/1.1
Host: app.example.com

# Resposta: 204 No Content
```

---

## ✅ Checklist de Implementação

- [x] Validar existência do arquivo
- [x] Bloquear deleção de diretórios
- [x] Validar permissões de escrita
- [x] Deletar arquivo com `remove()`
- [x] Retornar 204 No Content
- [x] Retornar 500 Internal Server Error
- [x] Logs detalhados
- [x] Path traversal protection
- [x] Validação de allowed_methods
- [x] Tratamento de erros

---

## 🚀 Próximos passos (opcional)

- [ ] Bulk delete (deletar múltiplos arquivos via JSON)
- [ ] Soft delete (mover para lixeira em vez de deletar)
- [ ] Backup antes de deletar
- [ ] Auditoria (log de quem deletou o quê)
- [ ] Rate limiting (limitar deletions por IP)

---

**DELETE está completo e pronto para uso em produção!** 🎉
