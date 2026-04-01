# 📚 Índice de Documentação do Webserver

**Última Atualização:** 1º de Abril de 2026  
**Status:** ✅ Documentação Completa (8 documentos, ~12K linhas)

---

## 🗺️ Mapa Mental de Documentos

```
┌─────────────────────────────────────────────────────────────┐
│                   DOCUMENTAÇÃO MAIN                         │
├─────────────────────────────────────────────────────────────┤
│ README.md (430 linhas) ← COMECE AQUI                       │
│ - Overview do projeto                                      │
│ - Funcionalidades implementadas vs não-implementadas       │
│ - Instruções de compilação e execução                      │
│ - Status do projeto e problemas conhecidos                 │
└──┬──────────────────────────────────────────────────────────┘
   │
   ├─ [Design Geral] ──> ARCHITECTURE.md (920 linhas)
   │                     • Visão de alto nível
   │                     • Componentes principais
   │                     • Fluxo de requisição
   │                     • Padrões de design
   │                     • Decisões críticas
   │
   ├─ [Debugar Problemas] ──> TROUBLESHOOTING.md (750 linhas)
   │                          • Problemas comuns
   │                          • Técnicas de debug
   │                          • Checklist
   │                          • GDB/Valgrind
   │
   └─ [Componentes] ────────┬────────────────────────────────┐
                           │                                │
              ┌────────────┴──────────┬─────────────┬───────┴────┐
              │                       │             │            │
              v                       v             v            v
        SERVER.md            CLIENT.md     RESPONSE.md    PARSING.md
        (1,845 linhas)      (1,330 linhas) (1,979 linhas) (1,978 linhas)
        
        • Orquestrador       • Máquina     • Geração      • ConfigParser
        • epoll loop           de estados    de respostas   • HttpRequest
        • Gerência de       • HTTP         • GET/POST/    • ConfigHelper
          conexões            buffering      DELETE       • Estruturas
        • Cleanup           • CGI          • Uploads      • Parsing
                            • Keep-Alive   • Security
                            
              │                       │             │            │
              └────────────┬──────────┴─────────────┴───────┬────┘
                          │                                │
                          v                                v
                      CGI.md                       IMPLEMENTATION.md
                      (2,500+ linhas)              (Opcional: detalhes)
                      
                      • Fork/pipes
                      • execve setup
                      • 31+ variáveis env
                      • Race conditions
```

---

## 📖 Guia de Leitura Recomendado

### Para Iniciantes (Seu Projeto)

**Objetivo:** Entender como o servidor funciona

1. **[README.md](README.md)** (10 min)
   - Overview e status
   - Funcionalidades implementadas
   - Instruções básicas
   
2. **[ARCHITECTURE.md](doc/ARCHITECTURE.md)** (20 min)
   - Componentes principais
   - Fluxo de requisição

3. **[SERVER.md](doc/SERVER.md)** (15 min)
   - Loop principal com epoll
   - Aceitar conexões

4. **[CLIENT.md](doc/CLIENTE.md)** (15 min)
   - Máquina de estados
   - Processamento HTTP

5. **[RESPONSE.md](doc/RESPONSE.md)** (15 min)
   - Geração de respostas
   - Métodos GET/POST/DELETE

6. **[PARSING.md](doc/PARSING.md)** (15 min)
   - Parse de configuração
   - Parse de requisição

7. **[CGI.md](doc/CGI.md)** (20 min)
   - Execução de scripts
   - Non-blocking I/O

8. **[TROUBLESHOOTING.md](doc/TROUBLESHOOTING.md)** (20 min)
   - Resolver problemas
   - Técnicas de debug

**Tempo Total:** ~2 horas para entendimento básico

---

### Para Debugar Problemas

1. **[TROUBLESHOOTING.md](doc/TROUBLESHOOTING.md)** ← AQUI PRIMEIRO
   - Procure seu problema na seção de problemas comuns
   - Siga o checklist específico

2. **Componente Relevante:**
   - GET não funciona? → [RESPONSE.md](doc/RESPONSE.md)
   - CGI não executa? → [CGI.md](doc/CGI.md)
   - Conexão cai? → [SERVER.md](doc/SERVER.md)
   - Parse falha? → [PARSING.md](doc/PARSING.md)

3. **[ARCHITECTURE.md](doc/ARCHITECTURE.md)**
   - Entender fluxo completo da requisição

---

### Para Adicionar Funcionalidade

**Novo endpoint?**
→ [PARSING.md](doc/PARSING.md) (locations) + [RESPONSE.md](doc/RESPONSE.md)

**Novo tipo de request (PUT)?**
→ [CLIENT.md](doc/CLIENTE.md) (máquina de estados) + [RESPONSE.md](doc/RESPONSE.md)

**Otimização de performance?**
→ [ARCHITECTURE.md](doc/ARCHITECTURE.md) (padrões) + [SERVER.md](doc/SERVER.md) (event loop)

**Novo formato de config?**
→ [PARSING.md](doc/PARSING.md) (ConfigParser)

---

## 📋 Descrição de Cada Documento

### README.md (Raiz)
- **Tamanho:** 430 linhas
- **Tempo de leitura:** 10 minutos
- **Propósito:** Overview completo do projeto
- **Seções:**
  - Descrição geral
  - Status do projeto e problemas conhecidos
  - Funcionalidades (implementadas vs não)
  - Tecnologias utilizadas
  - Instruções de compilação/execução/teste
  - Uso de IA durante desenvolvimento
  
**Quando ler:**
- Primeira coisa ao começar
- Para entender o scope do projeto
- Para saber como compilar e rodar

---

### ARCHITECTURE.md (doc/)
- **Tamanho:** 920 linhas
- **Tempo de leitura:** 20 minutos
- **Propósito:** Visão de arquitetura de alto nível
- **Seções:**
  - Visão geral com números-chave
  - Componentes principais (Server, Client, etc)
  - Fluxo de requisição completo (9 fases)
  - Arquitetura em camadas
  - Padrões de design (máquina de estados, dois-mapas, etc)
  - Decisões críticas (epoll vs poll, fork vs threads)
  - Recursos implementados vs não-implementados

**Quando ler:**
- Para entender como o sistema funciona como um todo
- Para tomar decisões de design
- Para referência rápida de componentes

---

### SERVER.md (doc/)
- **Tamanho:** 1.845 linhas
- **Tempo de leitura:** 30 minutos
- **Propósito:** Documentação profunda da classe Server
- **Seções:**
  - Propósito e responsabilidades
  - Arquitetura do Server
  - Loop principal de epoll
  - Membros privados (11+)
  - Métodos principais (start, addClient, handleClientData, etc)
  - 9 fases de cleanup
  - Fluxograma ASCII completo
  - Race conditions e sincronização
  - Gaps e limitações conhecidas

**Quando ler:**
- Entender event loop e multiplexing
- Debugar problemas de conexão
- Implementar novos tipos de eventos
- Melhorar performance

---

### CLIENT.md (doc/CLIENTE.md)
- **Tamanho:** 1.330 linhas
- **Tempo de leitura:** 25 minutos
- **Propósito:** Máquina de estados de cada conexão HTTP
- **Seções:**
  - Enum State (7 estados)
  - Fluxo de estados completo
  - Membros privados (16)
  - Membros de CGI
  - Métodos principales (20+)
  - Métodos CGI (6)
  - 7 problemas significativos
  - Gaps revisados

**Quando ler:**
- Entender protocol HTTP/1.1
- Keep-Alive não funciona?
- CGI não executa?
- Debugar timeout/estado

---

### RESPONSE.md (doc/)
- **Tamanho:** 1.979 linhas
- **Tempo de leitura:** 30 minutos
- **Propósito:** Geração de respostas HTTP
- **Seções:**
  - Processar GET (arquivos, diretórios, CGI)
  - Processar POST (uploads multipart)
  - Processar DELETE
  - 5 camadas de segurança
  - Validações (path traversal, symlinks)
  - Directory listing com sorting
  - Upload com rollback
  - Gaps de implementação

**Quando ler:**
- GET não returna arquivo?
- POST/upload não funciona?
- DELETE não deleta?
- Entender validações de segurança

---

### PARSING.md (doc/)
- **Tamanho:** 1.978 linhas
- **Tempo de leitura:** 30 minutos
- **Propósito:** Parse de configuração e HTTP
- **Seções:**
  - ConfigParser (parse de .conf)
  - HttpRequest (parse de HTTP/1.1)
  - ConfigHelper
  - ConfigValidator
  - StringUtils
  - Estruturas de dados (ServerConfig, LocationConfig)
  - Exemplo de configuração
  - Gaps e limitações

**Quando ler:**
- Adicionar nova diretiva de config?
- HTTP header não reconhecido?
- Parse de URL com query string?

---

### CGI.md (doc/)
- **Tamanho:** 2.500+ linhas
- **Tempo de leitura:** 35 minutos
- **Propósito:** Implementação detalhada de CGI
- **Seções:**
  - Configuração CGI no parser
  - Máquina de estados CGI (7 estados)
  - Fork/pipes/execve em detalhes
  - Variáveis de environment (31+)
  - Non-blocking I/O com epoll
  - Race conditions e sincronização
  - Problemas identificados (7 críticos)
  - Exemplos de scripts

**Quando ler:**
- CGI retorna erro 502?
- Variável de env não setada?
- Script CGI não executa?
- Entender fork/pipes/exec

---

### TROUBLESHOOTING.md (doc/)
- **Tamanho:** 750 linhas
- **Tempo de leitura:** 20 minutos
- **Propósito:** Diagnostic e resolução de problemas
- **Seções:**
  - Problemas comuns (7+) com checklists
  - Técnicas de debug (GDB, Valgrind, strace)
  - Checklist de compilação
  - Problemas de performance
  - Problemas de segurança
  - Análise de crashes
  - Recursos de diagnóstico
  
**Quando ler:**
- Servidor não inicia?
- Requisição fica pendurada?
- Performance é lenta?
- Crash ou segmentation fault?

---

## 🔍 Navegação Rápida por Problema

| Problema | Documento | Seção |
|----------|-----------|-------|
| Servidor não inicia | TROUBLESHOOTING | Problema 1 |
| Requisição não responde | TROUBLESHOOTING | Problema 2 |
| CGI retorna 502 | TROUBLESHOOTING | Problema 3 |
| Upload não funciona | TROUBLESHOOTING | Problema 4 |
| 403 Forbidden | TROUBLESHOOTING | Problema 5 |
| Keep-Alive não funciona | TROUBLESHOOTING | Problema 6 |
| Servidor fecha (SIGPIPE) | TROUBLESHOOTING | Problema 7 |
| Entender epoll | SERVER | Seção Event Loop |
| Máquina de estados HTTP | CLIENT | Enum State |
| GET/POST/DELETE | RESPONSE | Métodos principais |
| Configuração de server | PARSING | ConfigParser |
| CGI não funciona | CGI | Seções 1-3 |
| Performance lenta | TROUBLESHOOTING + SERVER | Seções respectivas |
| Memory leak | TROUBLESHOOTING | Técnicas de debug |

---

## 📊 Estatísticas de Documentação

| Documento | Linhas | Status | Última Atualização |
|-----------|--------|--------|-------------------|
| README.md | 430 | ✅ Atualizado | 1º Abril 2026 |
| ARCHITECTURE.md | 920 | ✅ Novo | 1º Abril 2026 |
| SERVER.md | 1.845 | ✅ Completo | 31 Março 2026 |
| CLIENT.md | 1.330 | ✅ Completo | 31 Março 2026 |
| RESPONSE.md | 1.979 | ✅ Completo | 31 Março 2026 |
| PARSING.md | 1.978 | ✅ Completo | 31 Março 2026 |
| CGI.md | 2.500+ | ✅ Completo | 31 Março 2026 |
| TROUBLESHOOTING.md | 750 | ✅ Novo | 1º Abril 2026 |
| **TOTAL** | **~12.0K** | ✅ | 1º Abril 2026 |

---

## 🎯 Checklist de Documentação

- ✅ Overview completo (README.md)
- ✅ Arquitetura de alto nível (ARCHITECTURE.md)
- ✅ Documentação de cada componente (8 docs)
- ✅ Fluxo de requisição (ARCHITECTURE.md + others)
- ✅ Exemplo de configuração (PARSING.md)
- ✅ Guia de troubleshooting (TROUBLESHOOTING.md)
- ✅ Técnicas de debug (TROUBLESHOOTING.md)
- ✅ Problemas conhecidos (README.md + docs)
- ✅ Padrões de decisão (ARCHITECTURE.md)
- ✅ Índice centralizado (este arquivo)

---

## 📚 Recursos Externos Recomendados

### Livros
- "Unix Network Programming" - Stevens & Fenner
- "The Linux Programming Interface" - Michael Kerrisk
- "HTTP/1.1 Specification" - RFC 2616

### Online
- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/)
- [NGINX Documentation](https://nginx.org/en/docs/)
- [Apache HTTP Server Documentation](https://httpd.apache.org/docs/)
- [Linux man pages](https://man7.org/)

### Ferramentas
- **Debugging:** GDB, Valgrind, strace
- **Testing:** curl, Apache Bench (ab), Wireshark
- **Performance:** perf, valgrind, cachegrind

---

## 🚀 Próximos Passos

### Gerar Documentação HTML (Opcional)

```bash
# Usando pandoc
pandoc -f markdown -t html README.md > docs/README.html
pandoc -f markdown -t html doc/ARCHITECTURE.md > docs/ARCHITECTURE.html

# Ou gerar um site completo
# (use mkdocs, sphinx, ou outro gerador)
```

### Melhorias Futuras

- [ ] Adicionar diagramas UML
- [ ] Criar vídeo tutorial
- [ ] Adicionar testes de unidade documentados
- [ ] Criar exemplo de extensão do servidor
- [ ] Performane profiling documentado

---

## 📞 Contato / Contribuições

Este projeto foi desenvolvido como parte do currículo da 42 por:
- **nmatondo**
- **ajacinto**
- **emalungo**

Para contribuições ou melhorias à documentação, abrir um pull request ou issue.

---

**Última Revisão:** 1º de Abril de 2026  
**Próxima Revisão Recomendada:** Após alterações significativas no código
