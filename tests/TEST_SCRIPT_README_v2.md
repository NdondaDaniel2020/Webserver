# Script de Teste do Webserver - v2.0

## 🎯 Visão Geral

O script `test_webserver.sh` v2.0 é uma ferramenta avançada e completa para validar o funcionamento do webserver. Executa **37 testes** organizados em **14 suites**, com foco especial em:

- ✅ Métodos HTTP (GET, POST, DELETE robustos)
- ✅ Validação de Headers e Content-Type
- ✅ Testes de Performance com timing
- ✅ Connection Management
- ✅ Upload/Download com multipart
- ✅ Tratamento de erros HTTP

## 🚀 Como Usar

### Uso Básico
```bash
./test_webserver.sh
```

### Requisitos
- Bash shell
- `curl` instalado
- `make` (para compilação)
- Arquivo de configuração: `config/default.conf`

## 📊 Estatísticas de Execução

**Resultado Típico** (servidor funcionando corretamente):
```
  ✓ Passaram:     32
  ✗ Falharam:     0
  ⚠ Avisos :     5
  ━ Total   :     37
```

**Tempo de Execução**: ~30 segundos
**Performance**: ~4ms por requisição

## 📋 Suite de Testes Detalhada

### ✓ Suite 1: Verificação de Pré-requisitos
- Binário compilado
- Arquivo de configuração
- Curl disponível

### ✓ Suite 2: Iniciação do Servidor
- Compila se necessário
- Inicia em background
- Aguarda resposta (máx 30s)

### ✓ Suite 3: Conectividade HTTP (2 testes)
- `GET /` - página inicial
- `GET /index.html` - arquivo estático

### ✓ Suite 4: Status Codes HTTP (4 testes)
- **2xx**: `200 OK`
- **3xx**: `301/302/304` Redirecionamento
- **4xx**: `404 Not Found`, `403 Forbidden`

### ✓ Suite 5: Validação de Conteúdo (2 testes)
- HTML com tags válidas
- Estrutura básica (html, body, head)

### ✓ Suite 6: Métodos HTTP (11 testes)

#### GET (2 testes)
- `GET /` raiz
- `GET /index.html` arquivo

#### POST (6 testes)
- Form URL encoded: `/`, `/api/submit`
- JSON: `/`, `/api/data`
- Multipart upload: `/uploads/`
- Sem Content-Type

#### DELETE (4 testes)
- `/` (raiz)
- `/uploads/` (diretório)
- `/uploads/test.txt` (arquivo)
- `/index.html` (protegido)

### ✓ Suite 7: Headers HTTP (5 testes)
- Content-Type
- Server header
- Content-Length
- Date header
- Status line

### ✓ Suite 8: Connection Management (1 teste)
- 5 requisições consecutivas

### ✓ Suite 9: Performance (2 testes)
- 20 requisições com timing
- Tempo médio por requisição

### ✓ Suite 10: Timeout (1 teste)
- Requisição com timeout 5s

### ✓ Suite 11: Diretórios (4 testes)
- `/` raiz
- `/text/` subdiretório
- `/api/` API
- `/uploads/` uploads

### ✓ Suite 12: Tipos de Arquivo (0 testes informativos)
- Detecção CSS/JS

### ✓ Suite 13: Páginas de Erro (2 testes)
- 404 Not Found
- 403 Forbidden

### ✓ Suite 14: Limites de Request (2 testes informativos)
- Corpo pequeno (5 bytes)
- Corpo moderado (~100KB)

## 🎨 Legenda Visual

| Símbolo | Significado |
|---------|-----------|
| ✓ | Teste passou |
| ✗ | Teste falhou |
| ⚠ | Aviso/alternativa |
| ℹ | Informação |
| → | Passo |
| ▶ | Sub-seção |

## 📋 Cores na Saída

- 🟢 **Verde**: Sucesso
- 🔴 **Vermelho**: Erro
- 🟡 **Amarelo**: Aviso
- 🔵 **Azul**: Info
- 🟣 **Magenta**: Headers

## 💻 Código de Saída

- **0** - Testes passaram ✓
- **1** - Testes falharam ✗

## 🔧 Personalizar

Edite no início do script:

```bash
SERVER_HOST="127.0.0.1"
SERVER_PORT="8080"
CONFIG_FILE="config/default.conf"
SERVER_BIN="./webserv"
```

## 🆘 Troubleshooting

### "Servidor não respondeu"
```bash
# Recompile
make clean && make

# Verifique porta em uso
lsof -i :8080

# Veja log
tail -50 /tmp/webserver_test.log
```

### "curl não instalado"
```bash
sudo apt-get install curl  # Ubuntu/Debian
brew install curl          # macOS
```

### "Algumas requisições falharam"
```bash
# Verificar log
cat /tmp/webserver_test.log

# Testar manualmente
curl -v http://127.0.0.1:8080/
```

## 📚 Referência de HTTP Status

| Código | Significado | Suite |
|--------|------------|-------|
| 200 | OK | 4, 6 |
| 201 | Created | 6 |
| 204 | No Content | 6 |
| 301 | Moved Permanently | 4 |
| 302 | Found | 4 |
| 304 | Not Modified | 4 |
| 400 | Bad Request | 6 |
| 403 | Forbidden | 4, 6, 13 |
| 404 | Not Found | 4, 13 |
| 415 | Unsupported Media | 14 |

## 🔄 Versões

**v1.0**: Testes básicos (19 testes)
**v2.0** (atual): Avançado, POST/DELETE, performance (37 testes)

### Mudanças em v2.0
- ✨ Removido: HEAD tests
- ✨ Adicionado: POST completo (form, JSON, multipart)
- ✨ Adicionado: DELETE robustos
- ✨ Adicionado: Performance com timing
- ✨ Adicionado: Upload de arquivos
- ✨ UI melhorada com cores
- ✨ 14 suites vs 12
- ✨ 37 testes vs 19

## 🎓 O que Valida

✅ Conectividade (localhost:8080)
✅ Métodos HTTP (GET, POST, DELETE)
✅ Status codes corretos
✅ Headers essenciais
✅ HTML válido
✅ Performance aceitável
✅ Upload de arquivos
✅ Redirecionamentos
✅ Páginas de erro
✅ Connection management

## ❌ O que NÃO Testa

- Autenticação
- CGI/Scripting
- HTTPS/SSL
- IPv6
- HTTP/2
- Rate limiting
- Load balancing

## 📄 Arquivo de Log

```bash
# Ver últimas linhas
tail -50 /tmp/webserver_test.log

# Ver apenas erros
grep -i "error\|failed" /tmp/webserver_test.log
```

## 📞 Extensões Futuras

- Teste de autenticação
- Teste de CGI
- Teste de HTTPS
- Teste de gzip
- Teste de cookies
- Teste de segurança

---

**Versão**: 2.0  
**Data**: 28 de fevereiro de 2026  
**Status**: ✓ Operacional
