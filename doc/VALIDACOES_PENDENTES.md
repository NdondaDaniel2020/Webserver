# Validações Pendentes - Parser de Configuração

## Objetivo
Evitar crashes e comportamentos indefinidos causados por configurações inválidas, testes malformados dos avaliadores ou entrada de dados inesperada.

---

## 1. VALIDAÇÕES DE SINTAXE E ESTRUTURA

### 1.1 Linhas vazias ou com apenas whitespace fora de blocos
- **Status**: Parcialmente feito
- **Problema**: Conteúdo inválido dentro de um bloco pode passsar despercebido
- **Solução**: Validar que toda linha dentro de um bloco tem formato válido (key value;)

### 1.2 Chaves duplicadas no mesmo bloco
- **Status**: Não feito
- **Problema**: `listen 127.0.0.1:8080` duas vezes pode causar comportamento indefinido
- **Solução**: Tracking de chaves já encontradas por server/location

### 1.3 Valores vazios após a chave
- **Status**: Parcialmente feito
- **Problema**: `listen ;` ou `root ;` podem crashar
- **Solução**: Validar que value não é vazio após trim

---

## 2. VALIDAÇÕES DE VALORES

### 2.1 Números inválidos em campos numéricos
- **Status**: Parcialmente feito
- **Problema**: `client_max_body_size abc` ou `listen 999999:999999`
- **Solução**: Validar que port está entre 1-65535, body_size > 0

### 2.2 Portas duplicadas entre múltiplos servers
- **Status**: Não feito
- **Problema**: Dois servers na mesma porta causam crash ao iniciar
- **Solução**: Track de (interface:port) já usados

### 2.3 Interface/IP inválida
- **Status**: Parcialmente feito
- **Problema**: `listen 256.300.400.500:8080` não é validado
- **Solução**: Usar regex ou inet_pton para validar IPs

### 2.4 Valores booleanos inválidos
- **Status**: Não feito
- **Problema**: `autoindex maybe` ou `autoindex 1`
- **Solução**: Aceitar apenas "on" ou "off"

### 2.5 Códigos HTTP inválidos
- **Status**: Parcialmente feito
- **Problema**: `return 999 http://...` ou `error_page abc /error.html`
- **Solução**: Whitelist de códigos válidos (3xx, 4xx, 5xx)

---

## 3. VALIDAÇÕES DE PATHS

### 3.1 Paths vazios
- **Status**: Não feito
- **Problema**: `location //` ou `root ""`
- **Solução**: Rejeitar paths vazios em location, root, error_page

### 3.2 Paths com caracteres inválidos
- **Status**: Não feito
- **Problema**: `location /path\0invalid` ou `root /path|pipe`
- **Solução**: Validar caracteres permitidos (alfanuméricos, /, -, _, .)

### 3.3 Paths duplicados em locations do mesmo server
- **Status**: Não feito
- **Problema**: Dois `location /uploads` no mesmo server
- **Solução**: Track de paths já encontrados

### 3.4 Paths absolutos vs relativos inconsistentes
- **Status**: Parcialmente feito
- **Problema**: Misturar roots absolutos com relativos
- **Solução**: Padronizar tratamento

### 3.5 Redirecionamento para paths inválidas
- **Status**: Não feito
- **Problema**: `return 301 ""` ou `return 301 /path\\with\\bad\\chars`
- **Solução**: Validar URL/path do redirecionamento

---

## 4. VALIDAÇÕES DE CONFIGURAÇÕES CGI

### 4.1 Extensões CGI duplicadas no mesmo location
- **Status**: Não feito
- **Problema**: `cgi_extension .py` duas vezes
- **Solução**: Track de extensões já encontradas

### 4.2 Caminhos de executores inválidos
- **Status**: Não feito
- **Problema**: `cgi_path /usr/bin/inexistent_interpreter`
- **Solução**: Validar se arquivo existe e é executável (ou alertar)

### 4.3 Extensões CGI mal formatadas
- **Status**: Não feito
- **Problema**: `cgi_extension py` (sem ponto) ou `cgi_extension .py.backup`
- **Solução**: Validar formato .extensão

### 4.4 Combinação CGI + redirect
- **Status**: Não feito
- **Problema**: `return 301` e `cgi_extension` no mesmo location
- **Solução**: Rejeitar essa combinação

---

## 5. VALIDAÇÕES DE UPLOAD

### 5.1 Upload_dir fora do root
- **Status**: Parcialmente feito
- **Problema**: `root /www` mas `upload_dir /var/upload`
- **Solução**: Validar que upload_dir é relativo ao root

### 5.2 Permissions de upload_dir
- **Status**: Não feito
- **Problema**: Diretório sem permissão de escrita
- **Solução**: Validar permissões ou alertar

### 5.3 allowed_methods sem POST em upload
- **Status**: Não feito
- **Problema**: `upload_dir` definido mas sem `allowed_methods POST`
- **Solução**: Validar que POST está em allowed_methods

---

## 6. VALIDAÇÕES DE ERROR_PAGES

### 6.1 Error pages com códigos inválidos
- **Status**: Parcialmente feito
- **Problema**: `error_page 199 /error.html`
- **Solução**: Validar apenas 3xx, 4xx, 5xx

### 6.2 Error pages duplicadas para mesmo código
- **Status**: Não feito
- **Problema**: `error_page 404 /404.html` duas vezes
- **Solução**: Última valor vence ou rejeitar duplicata

### 6.3 Arquivo de error page não existe
- **Status**: Não feito
- **Problema**: `error_page 404 /does_not_exist.html`
- **Solução**: Alertar ou validar existência

---

## 7. VALIDAÇÕES DE INDEX

### 7.1 Index vazio
- **Status**: Não feito
- **Problema**: `index ;` ou `index ""`
- **Solução**: Rejeitar index vazio

### 7.2 Múltiplos index (possível conflito)
- **Status**: Parcialmente feito
- **Problema**: Comportamento indefinido com múltiplos indices
- **Solução**: Documentar prioridade ou validar limites

---

## 8. VALIDAÇÕES DE SERVER_NAME

### 8.1 Server names duplicados
- **Status**: Não feito
- **Problema**: Dois servers com mesmo server_name e porta
- **Solução**: Avisar ou rejeitar duplicata

### 8.2 Server name inválido
- **Status**: Parcialmente feito
- **Problema**: `server_name !!!invalid!!!`
- **Solução**: Validar contra RFC de DNS/hostnames

### 8.3 Server name vazio
- **Status**: Não feito
- **Problema**: `server_name "";`
- **Solução**: Rejeitar ou usar padrão

---

## 9. VALIDAÇÕES DE TAMANHO E LIMITES

### 9.1 client_max_body_size com valor irreal
- **Status**: Não feito
- **Problema**: `client_max_body_size 999999999999GB`
- **Solução**: Limitar a valores razoáveis (ex: máx 10GB)

### 9.2 Tamanho negativo
- **Status**: Não feito
- **Problema**: `client_max_body_size -10M`
- **Solução**: Rejeitar valores negativos

### 9.3 Unidades inválidas
- **Status**: Parcialmente feito
- **Problema**: `client_max_body_size 10XB`
- **Solução**: Validar apenas: B, K, M, G

---

## 10. VALIDAÇÕES LÓGICAS

### 10.1 Redirect vs conteúdo
- **Status**: Não feito
- **Problema**: Location com `return 301` E `root` definido
- **Solução**: Rejeitar essa combinação

### 10.2 Location com apenas redirect sem outras configs
- **Status**: Não feito
- **Problema**: `location /path { return 301 /other; }` pode estar incompleto
- **Solução**: Aceitar ou documentar comportamento

### 10.3 Root não definido ou vazio
- **Status**: Não feito
- **Problema**: Server sem `root` definido
- **Solução**: Usar padrão ou rejeitar

### 10.4 Index não definido
- **Status**: Não feito
- **Problema**: Server sem `index` definido
- **Solução**: Usar padrão como index.html

---

## 11. VALIDAÇÕES DE CONSISTÊNCIA GLOBAL

### 11.1 Pelo menos um server deve estar definido
- **Status**: Feito ✓

### 11.2 Servidor sem location
- **Status**: Não feito
- **Problema**: Server sem nenhum location definido é válido?
- **Solução**: Documentar ou validar

### 11.3 Circular references em redirects
- **Status**: Não feito
- **Problema**: `/a` redireciona para `/b`, `/b` redireciona para `/a`
- **Solução**: Detectar ou ignorar (o cliente lida com depois)

---

## 12. VALIDAÇÕES DE ROBUSTEZ

### 12.1 Linhas muito longas
- **Status**: Não feito
- **Problema**: Buffer overflow ou performance issues
- **Solução**: Limitar comprimento de linha (ex: 4KB)

### 12.2 Totalidade de linhas no arquivo
- **Status**: Não feito
- **Problema**: Arquivo com bilhões de linhas
- **Solução**: Limitar número de linhas (ex: máx 10K linhas)

### 12.3 Profundidade de aninhamamento
- **Status**: Parcialmente feito (apenas 2 níveis: server > location)
- **Problema**: Blocos aninhados infinitamente
- **Solução**: Rejeitar profundidade > 2

### 12.4 Fechamento de blocos
- **Status**: Parcialmente feito
- **Problema**: Bloco aberto sem fechar `}`
- **Solução**: Já validado (EOF sem `}` = erro)

---

## PRIORIDADE RECOMENDADA

### CRÍTICAS (Evitam crashes certos)
1. Números inválidos em campos numéricos
2. Portas duplicadas entre servers
3. Paths vazios em location
4. Códigos HTTP inválidos
5. Chaves duplicadas no mesmo bloco

### ALTAS (Comportamento indefinido)
6. Server names duplicados
7. Paths duplicados em locations
8. Extensões CGI duplicadas
9. Combinações inválidas (redirect + root)
10. Error page codes inválidos

### MÉDIAS (Melhor UX e debugging)
11. Interface/IP inválida
12. Valores booleanos inválidos
13. Extensões CGI mal formatadas
14. Caracteres inválidos em paths
15. Limites de tamanho realistas

### BAIXAS (Nice-to-have)
16. Validar existência de arquivos CGI
17. Validar permissões de diretórios
18. Detectar circular references
19. Circular Redirecionamentos

---

## IMPLEMENTAÇÃO RECOMENDADA

1. **Pré-parsing**: Validações de sintaxe básica (linhas, caracteres)
2. **Durante parsing**: Validações de valores (números, ranges, formatos)
3. **Pós-parsing**: Validações lógicas e de consistência global
4. **Log detalhado**: Cada validação deve indicar linha e setor no config

