# Índice de Vulnerabilidades - Análise Consolidada

**Data da Análise:** 22/03/2026  
**Status Geral:** 44% das falhas críticas e altas resolvidas (19/43)

---

## 📊 Resumo Executivo

| Arquivo | Falhas | ✅ OK | ❌ Pendentes | Taxa |
|---------|--------|-------|------------|------|
| [FD_LEAKS.md](FD_LEAKS.md) | 6 | 6 | 0 | ✅ 100% |
| [FALHAS_CLIENTE.md](FALHAS_CLIENTE.md) | 6 | 3 | 3 | ⏳ 50% |
| [FALHAS_CGI.md](FALHAS_CGI.md) | 11 | 2 | 9 | ❌ 18% |
| [FALHAS_RESPONSE.md](FALHAS_RESPONSE.md) | 5 | 0 | 5 | ❌ 0% |
| [FALHAS_SERVER.md](FALHAS_SERVER.md) | 15 | 8 | 7 | ⏳ 53% |
| **TOTAL** | **43** | **19** | **24** | **⏳ 44%** |

---

## 🔴 CRÍTICAS - Implementar ASAP (5 máximas prioridades)

### 1. **FALHAS_RESPONSE.md::Falha 1** - readFile sem limite  
- **Risco:** Memory DoS (1 cliente com arquivo 10GB trava servidor)
- **Localização:** FileUtils.cpp:22-32
- **Solução:** Adicionar `MAX_FILE_SIZE_LIMIT` (10-100MB)

### 2. **FALHAS_SERVER.md::Falha 9** - sendTimeoutResponse bloqueia
- **Risco:** Server DoS completo (1 cliente malicioso paralisa)
- **Localização:** Server.cpp:364-385
- **Solução:** Usar send_buffer normal (deixar EPOLLOUT enviar)

### 3. **FALHAS_CGI.md::Falha 1** - CGI sem limite de memória
- **Risco:** Memory DoS (script infinito consome RAM)
- **Localização:** Client.cpp:774, 805
- **Solução:** Adicionar `MAX_CGI_OUTPUT_SIZE` (10MB)

### 4. **FALHAS_RESPONSE.md::Falha 3** - TOCTOU methodDelete
- **Risco:** Arbitrary file deletion via symlink race
- **Localização:** Response.cpp:296-360
- **Solução:** Usar `unlinkat()` atômico com AT_SYMLINK_NOFOLLOW

### 5. **FALHAS_CGI.md::Falha 2** - Ignora status de saída
- **Risco:** Execução falha passa como 200 OK
- **Localização:** Client.cpp:800-810
- **Solução:** Usar `WIFEXITED/WIFSIGNALED/WTERMSIG` macros

---

## 🟠 ALTAS - Implementar esta semana

### FALHAS_SERVER.md
- Falha 5: accept() sem IP cliente → Rate limiting/auditoria
- Falha 10: Sem limite MAX_CLIENTS → Connection flood DoS
- Falha 6: closeClient ordem → Conceitualmente correto

### FALHAS_CGI.md
- Falha 3: Race condition pipes → Output pode ser perdido
- Falha 4: stderr misturado → Headers HTTP malformados
- Falha 5: write stdin erro → Cliente fica preso no timeout
- Falha 7: Sem validar interpretador → Desperdício de fork

### FALHAS_RESPONSE.md
- Falha 2: Path traversal multipart → Arbitrary file write
- Falha 4: JSON injection → XSS
- Falha 5: Sem limite multipart → DoS exhaustion

---

## 🟡 MÉDIAS - Próximas iterações

### FALHAS_SERVER.md
- Falha 12: Double sendData race → Code cleanup
- Falha 13: checkTimeout O(n) → Otimizar para O(log n)
- Falha 14: epoll_wait timeout alto → Reduzir de 1s para 100ms

### FALHAS_CGI.md
- Falha 6: Timeout hardcoded 10s → Configurável
- Falha 8: Parsing headers frágil → Validar formato
- Falha 9: Sem detecção sinais → Diagnosticar crashes

### FALHAS_CLIENTE.md
- Falha 1: Destrutor não valida CGI → Avisos defensivos
- Falha 2: Copy constructor perigoso → Disabled/delete
- Falha 3: reset() não limpa CGI → Adicionar cleanup

---

## ✅ RESOLVIDAS - Validades

### FD_LEAKS.md (100%)
- ✅ CASO 1-6: Todos destrutor, CLOEXEC, error paths, try-catch

### FALHAS_CLIENTE.md (Parcial)
- ✅ FALHA 4: CLOEXEC em child FDs
- ✅ FALHA 5: startCgi fecha pipes em 7+ error paths
- ✅ FALHA 6: sendData diferencia EAGAIN

### FALHAS_SERVER.md (Parcial)
- ✅ FALHA 1: Destrutor limpa recursos
- ✅ FALHA 2: Construtor lança exceção
- ✅ FALHA 3: new[] com cleanup
- ✅ FALHA 6: epoll_ctl verificado
- ✅ FALHA 7: fcntl verificado
- ✅ FALHA 8: new Client com try/catch

### FALHAS_CGI.md (Mínimo)
- ✅ FALHA 10: Configuração não depende de ordem
- ✅ FALHA 11: Responsabilidades não duplicadas

---

## 🔗 Referências Cruzadas

### Por Tipo de Vulnerabilidade

**DoS (Denial of Service):**
- FALHAS_RESPONSE.md::1 - readFile sem limite
- FALHAS_CGI.md::1 - CGI output sem limite
- FALHAS_SERVER.md::9 - sendTimeoutResponse bloqueia
- FALHAS_SERVER.md::10 - Sem limit MAX_CLIENTS

**Arbitrary Write (Path Traversal):**
- FALHAS_RESPONSE.md::2 - multipart filename
- FALHAS_RESPONSE.md::5 - multipart count

**Arbitrary Delete:**
- FALHAS_RESPONSE.md::3 - TOCTOU metodDelete

**Information Leak:**
- FALHAS_CGI.md::4 - stderr em stdout
- FALHAS_CGI.md::9 - sem detecção sinais

**Resource Leak:**
- FALHAS_CLIENTE.md::1 - destrutor CGI
- FALHAS_CGI.md::3 - race pipes

**Race Conditions:**
- FALHAS_CGI.md::3 - pipe registration
- FALHAS_RESPONSE.md::3 - TOCTOU delete
- FALHAS_SERVER.md::12 - double sendData

**Code Injection:**
- FALHAS_RESPONSE.md::4 - JSON injection

---

## 📝 Como Usar Este Documento

1. **Para começar implementação:** Veja seção 🔴 CRÍTICAS
2. **Para entender status:** Veja tabela inicial
3. **Para referência cruzada:** Veja seção Referências Cruzadas
4. **Para arquivo específico:** Use [Links] no corpo do texto

---

## 🛠️ Status das 4 Semanas

**Semana 1 (Análise):**
- ✅ FD_LEAKS.md analisado (100% completo)
- ✅ Todos 4 FALHAS_*.md documentados

**Semana 2 (Correções Críticas):**
- ⏳ readFile limit implementar
- ⏳ sendTimeoutResponse não bloquear
- ⏳ CGI output limit implementar

**Semana 3 (Correções Altas):**
- ⏳ MAX_CLIENTS implementar
- ⏳ accept IP implementar
- ⏳ metodDelete TOCTOU fix

**Semana 4 (Refinamentos):**
- ⏳ checkTimeout otimizar
- ⏳ Validações e testes
- ⏳ Documentação final

---

## 📚 Arquivos de Referência

- [FD_LEAKS.md](FD_LEAKS.md) - File Descriptor leak scenarios (COMPLETO)
- [FALHAS_CGI.md](FALHAS_CGI.md) - CGI implementation issues (18% resolved)
- [FALHAS_CLIENTE.md](FALHAS_CLIENTE.md) - Client class vulnerabilities (50% resolved)
- [FALHAS_RESPONSE.md](FALHAS_RESPONSE.md) - HTTP response issues (0% resolved)
- [FALHAS_SERVER.md](FALHAS_SERVER.md) - Server managing vulnerabilities (53% resolved)
- [README.md](README.md) - Main project documentation
