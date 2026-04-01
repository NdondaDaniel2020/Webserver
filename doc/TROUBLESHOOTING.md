# 🔧 GUIA DE TROUBLESHOOTING E DEBUG

**Data de Criação:** 1º de Abril de 2026  
**Última Atualização:** 1º de Abril de 2026

---

## 📋 Índice

1. [Problemas Comuns](#problemas-comuns)
2. [Técnicas de Debug](#técnicas-de-debug)
3. [Checklist de Compilação](#checklist-de-compilação)
4. [Problemas de Performance](#problemas-de-performance)
5. [Problemas de Segurança](#problemas-de-segurança)
6. [Análise de Crashes](#análise-de-crashes)
7. [Recursos de Diagnóstico](#recursos-de-diagnóstico)

---

## Problemas Comuns

### 1. Servidor Não Inicia

**Sintoma:** `./webserv config.conf` não retorna nada ou crash imediato

**Checklist:**
```bash
# 1. Verificar sintaxe do arquivo de configuração
cat config/default.conf

# 2. Verificar se arquivo config existe
ls -la config/default.conf

# 3. Compilar com debug symbols
make fclean && make

# 4. Rodar com strace para ver syscalls
strace -e trace=file,net,process ./webserv config/default.conf 2>&1 | head -50

# 5. Verificar se porta está em uso
netstat -tlnp | grep 8080
# ou
ss -tlnp | grep 8080
```

**Possíveis Causas:**

| Problema | Solução |
|----------|---------|
| Porta já em uso | Matar proceso anterior ou usar porta diferente |
| Arquivo config não existe | Criar/verificar caminho do arquivo |
| Permissão negada em diretório | Verificar permissões (`chmod`) |
| Sintaxe inválida em config | Revisar formato (bloco `server { ... }`) |
| Segmentation fault na init | Ver [Análise de Crashes](#análise-de-crashes) |

---

### 2. Requisições Não São Respondidas

**Sintoma:** `curl http://localhost:8080/` fica pendurado ou timeout

**Checklist:**
```bash
# 1. Verificar se servidor está rodando
ps aux | grep webserv

# 2. Testar port listener
telnet localhost 8080
# ou
nc -vz localhost 8080

# 3. Enviar requição HTTP simples
echo -e "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n" | nc localhost 8080

# 4. Usar muestraverbosity do curl
curl -vvv http://localhost:8080/

# 5. Verificar com strace o que o servidor está fazendo
strace -p $(pgrep webserv) -f
```

**Possíveis Causas:**

| Problema | Solução |
|----------|---------|
| epoll não registrou cliente | Verificar `epoll_ctl(ADD)` em newConnection() |
| Cliente em timeout aguardando headers | Enviar headers completos com `\r\n\r\n` |
| Buffer recv_buffer cheio | Aumentar tamanho ou depurar acúmulo |
| Estado do cliente incorreto | Verificar máquina de estados |
| SIGPIPE fecha servidor | Implementar signal handler para SIGPIPE |

**Debug Code:**
```cpp
// Em Server.cpp, adicionar prints
std::cerr << "[DEBUG] newConnection fd=" << client_fd << std::endl;
std::cerr << "[DEBUG] handleClientData fd=" << client_fd 
          << " bytes=" << nread << std::endl;
std::cerr << "[DEBUG] client state=" << client->getState() << std::endl;
```

---

### 3. CGI Não Executa ou Retorna Erro

**Sintoma:** Requisição para `.py` ou `.php` retorna 502 Bad Gateway ou 500

**Checklist:**
```bash
# 1. Verificar se interpretador existe
which python3
which php-cgi

# 2. Testar interpretador diretamente
echo 'print("Hello")' > test.py
python3 test.py

# 3. Verificar se script tem permissão execute
ls -la www/cgi-bin/test.py

# 4. Testar com curl incluindo body para POST
curl -X GET "http://localhost:8080/cgi-bin/test.py?param=value" -v

# 5. Verificar sa config tem cgi_path correto
grep -A5 "location /cgi-bin" config/default.conf
```

**Possíveis Causas:**

| Problema | Solução |
|----------|---------|
| Interpretador não encontrado | Usar caminho completo (ex: `/usr/bin/python3`) |
| cgi_path não configurado | Adicionar em config: `cgi_path /usr/bin/python3` |
| Script com erro | Testar script diretamente: `python3 script.py` |
| Permissionary denied | `chmod +x www/cgi-bin/script.py` |
| Timeout CGI (60s) | Verificar se script é muito lento |
| Variável de env não setada | Verificar EnvBuilder.cpp |

**Debug CGI:**
```cpp
// Em Client.cpp, antes de execve
std::cerr << "[CGI] Starting: " << cgi_path << " " << script_filename << std::endl;
std::cerr << "[CGI] Env:" << std::endl;
for (int i = 0; env[i]; i++) {
    std::cerr << "  " << env[i] << std::endl;
}

// Redirecionar stderr do script para debug
dup2(fileno(stderr), fileno(stderr)); // Se quiser ver stderr do script
```

---

### 4. Upload de Arquivo Não Funciona

**Sintoma:** `curl -X POST -F "file=@test.txt"` retorna erro ou arquivo não aparece

**Checklist:**
```bash
# 1. Verificar diretório de uploads
ls -la www/uploads/
chmod 777 www/uploads/

# 2. Testar POST com curl
curl -X POST -F "file=@/etc/hostname" -v http://localhost:8080/

# 3. Verificar se arquivo foi criado
ls -la www/uploads/

# 4. Verificar client_max_body_size em config
grep client_max_body_size config/default.conf

# 5. Testar com arquivo pequeno
echo "test" > /tmp/test.txt
curl -X POST -F "file=@/tmp/test.txt" -v http://localhost:8080/
```

**Possíveis Causas:**

| Problema | Solução |
|----------|---------|
| Extensão não permitida | Whitelist em Response.cpp (~11 tipos) |
| Diretório uploads não existe | Criar: `mkdir -p www/uploads` |
| Permissão negada em diretório | `chmod 777 www/uploads` |
| client_max_body_size excedido | Aumentar em config: `client_max_body_size 100M` |
| Parse multipart falha | Verificar boundary em Content-Type |
| Buffer 4KB insuficiente | Aumentar RECV_BUFFER_SIZE (problema conhecido) |

**Debug Upload:**
```cpp
// Em Response.cpp, método multipartFormData
std::cerr << "[UPLOAD] Boundary: " << boundary << std::endl;
std::cerr << "[UPLOAD] File: " << filename << ", Size: " << file_size << std::endl;
std::cerr << "[UPLOAD] Extension: " << ext << std::endl;
std::cerr << "[UPLOAD] Final path: " << filepath << std::endl;
```

---

### 5. Erro 403 Forbidden em Diretório

**Sintoma:** `curl http://localhost:8080/` retorna 403 mesmo com arquivo index

**Checklist:**
```bash
# 1. Verificar se index.html existe
ls -la www/index.html

# 2. Verificar permissões (legível)
test -r www/index.html && echo "readable" || echo "NOT readable"

# 3. Testar com arquivo direto
curl http://localhost:8080/index.html

# 4. Habilitar autoindex em config
grep -B2 -A5 "location /" config/default.conf

# 5. Verificar se '/' está em index_files
grep index_files config/default.conf
```

**Possíveis Causas:**

| Problema | Solução |
|----------|---------|
| Arquivo index não listado em config | Adicionar: `index index.html` |
| Autoindex desativado e sem index | Adicionar `autoindex on` OU criar index.html |
| Path traversal detectado | Verificar se path contém `..` |
| Permissões arquivo inlegível | `chmod +r www/index.html` |
| Diretório fora de root | Configurar corretamente `root www/` |

---

### 6. Keep-Alive Não Funciona

**Sintoma:** Cada requisição abre nova conexão (sem reutilização)

**Checklist:**
```bash
# 1. Verificar se servidor suporta Connection: Keep-Alive
curl -v http://localhost:8080/
# Procurar: "Connection: keep-alive"

# 2. Testar múltiplas requisições na mesma conexão
{
  echo -e "GET / HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n"
  sleep 1
  echo -e "GET /index.html HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n"
} | nc localhost 8080

# 3. Usar curl com -w para ver tempo de conexão
curl -w "time_connect: %{time_connect}s\n" http://localhost:8080/ \
  http://localhost:8080/index.html
```

**Possíveis Causas:**

| Problema | Solução |
|----------|---------|
| HTTP/1.0 retornado (não keep-alive) | Retornar HTTP/1.1 em response |
| Connection: close hardcoded | Remover/condicionar em Response.cpp |
| Client::reset() não funciona | Verificar se retorna a READING_HEADERS |
| Timeout desconecta | Aumentar timeout ou manter vivo |

---

### 7. Servidor Fecha Abruptamente (SIGPIPE)

**Sintoma:** Servidor morre quando cliente desconecta durante send()

**Checklist:**
```bash
# 1. Executar sem crash
./webserv config/default.conf &
SERVER_PID=$!

# 2. Iniciar requisição longa e desconectar rapidamente
(sleep 0.1; curl http://localhost:8080/ &) &
sleep 0.5
kill $SERVER_PID

# 3. Verificar se handler SIGPIPE existe com dmesg
dmesg | tail -20
```

**Possíveis Causas:**

| Problema | Solução |
|----------|---------|
| SIGPIPE não manuseado | Adicionar: `signal(SIGPIPE, SIG_IGN)` em main.cpp |
| send() retorna erro mas não verifica | Verificar erro de send() |
| Escrever em pipe fechado | Verificar EPIPE antes de send() |

**Fix in main.cpp:**
```cpp
#include <signal.h>

int main(int argc, char **argv) {
    // Ignorar SIGPIPE para não derrubar servidor
    signal(SIGPIPE, SIG_IGN);
    
    // ... resto do código
}
```

---

## Técnicas de Debug

### 1. Compilar com Debug Symbols

```bash
# Editar Makefile
CXXFLAGS = -Wall -Wextra -Werror -g -O0  # Adicionar -g

# Recompilar
make fclean && make
```

### 2. Usar GDB para Breakpoints

```bash
gdb ./webserv

# No GDB
(gdb) break Client.cpp:150
(gdb) run config/default.conf
(gdb) continue
# [Enviar requisição em outro terminal]

(gdb) print client->state
(gdb) print recv_buffer
(gdb) next
(gdb) step
```

### 3. Usar Valgrind para Detectar Vazamentos

```bash
# Detectar memory leaks
valgrind --leak-check=full --show-leak-kinds=all \
    ./webserv config/default.conf

# Em outro terminal (20-30 segundos)
for i in {1..10}; do
    curl http://localhost:8080/ &
done
wait

# Voltare ao valgrind e pressionar Ctrl+C
# Ver relatório de leaks
```

### 4. Usar strace para Rastrear Syscalls

```bash
# Rastrear todas as syscalls
strace -f ./webserv config/default.conf 2>&1 | head -100

# Rastrear apenas syscalls de rede
strace -e trace=network ./webserv config/default.conf

# Rastrear I/O
strace -e trace=read,write,recv,send ./webserv config/default.conf
```

### 5. Verificar File Descriptors Abertos

```bash
# Descobrir PID do servidor
SERVER_PID=$(pgrep webserv)

# Ver todos os FDs abertos
ls -la /proc/$SERVER_PID/fd/

# Contar FDs
ls -1 /proc/$SERVER_PID/fd/ | wc -l

# Ver tipos de FDs
lsof -p $SERVER_PID
```

### 6. Adicionar Logging de Debug

```cpp
// Em Client.cpp
#define DEBUG_LOG 1

#if DEBUG_LOG
#define LOG(x) std::cerr << "[" << fd << "] " << x << std::endl;
#else
#define LOG(x)
#endif

LOG("Estado: " << state << ", recv_buffer: " << recv_buffer.length());
```

### 7. Testar com tcpdump

```bash
# Capturar tráfego HTTP
tcpdump -A -l -n 'tcp port 8080' | grep -E 'GET|HTTP'

# Salvar para análise
tcpdump -w traffic.pcap 'tcp port 8080'

# Depois abrir no Wireshark
wireshark traffic.pcap
```

---

## Checklist de Compilação

### Verificações Antes de Compilar

```bash
# 1. Verificar dependências
which c++
which make

# 2. Verificar sintaxe C++98
grep -r "auto\|nullptr\|std::cout <<" src/ include/ || echo "C++98 OK"

# 3. Verificar includes
grep -r "#include" include/ | grep -E "<.*>" | head -10

# 4. Compilar e checar warnings
make clean
make 2>&1 | grep -E "warning|error" | head -20
```

### Problemas de Compilação Comuns

| Problema | Solução |
|----------|---------|
| `undefined reference to XXX` | Adicionar arquivo .cpp no Makefile |
| `no member named XXX` | Verificar spelling em .hpp |
| `-std=c++98 not supported` | Usar `c++ -std=c++98` em vez de `g++` |
| `fatal error: XXX.hpp: No such file` | Verificar path em #include |
| `multiple definition of XXX` | Remover implementation de .hpp |

---

## Problemas de Performance

### 1. Servidor Lento com Múltiplas Conexões

**Sintoma:** Performance degrada com 100+ conexões simultâneas

**Análise:**
```bash
# 1. Verificar CPU usage
top -p $(pgrep webserv)

# 2. Verificar I/O
iostat -x 1 | head -20

# 3. Testar com ab (Apache Bench)
ab -n 10000 -c 100 http://localhost:8080/

# 4. Profiling com perf
perf record -p $(pgrep webserv) -- sleep 10
# [Enviar requisições]
perf report
```

**Possíveis Causas:**

| Problema | Solução |
|----------|---------|
| Timeout O(n) | Usar heap ou deixar passivo |
| recv/send sem buffering | Usar buffer maior que 4KB |
| epoll_wait() com timeout fixo 1s | Usar timeout dinâmico baseado em próximo timeout |
| Logging excessivo | Remover ou desativar em release |
| Algoritmo O(n) em location match | Usar hash map ou trie |

---

### 2. Alto Uso de Memória

**Sintoma:** Servidor consome GBs de RAM

**Análise:**
```bash
# 1. Ver RSS memory
ps aux | grep webserv
# Columna RSS

# 2. Usar valgrind para detectar leaks
valgrind --leak-check=summary ./webserv config.conf

# 3. Usar pmap para ver segmentação
pmap -x $(pgrep webserv) | tail -10
```

**Possíveis Causas:**

| Problema | Solução |
|----------|---------|
| recv_buffer acumula | Limpar ou usar circular buffer |
| cgi.output não truncado | Limpar após envio |
| Clientes não deletados | Verificar closeClient() |
| Vazamento de memória | Ver Valgrind output |

---

## Problemas de Segurança

### 1. Path Traversal Detectado

```bash
# Testar segurança
curl http://localhost:8080/../../../../etc/passwd
# Esperado: 403 Forbidden ou 404 Not Found

# Verificar proteção
curl http://localhost:8080/../www/index.html
```

### 2. Extensão Não-Permitida Upload

```bash
# Testar whitelist
curl -X POST -F "file=@malware.exe" http://localhost:8080/
# Esperado: 415 Unsupported Media Type ou 403

# Extensões permitidas (~11):
# jpg, jpeg, png, gif, pdf, txt, doc, docx, zip, mp4, mp3
```

### 3. Content-Length Bombing

```bash
# Enviar Content-Length muito grande
echo "GET / HTTP/1.1" | nc localhost 8080 &
# Digitalmente neste caso, observador server não deve alocar 1GB
```

---

## Análise de Crashes

### GDB Quick Reference

```bash
# Rodando servidor
./webserv config/default.conf &
PID=$!

# Em outro terminal
gdb attach $PID
(gdb) continue

# [Reprodilo crash...]

(gdb) bt  # backtrace
(gdb) print variable_name
(gdb) frame 0
(gdb) list
```

### Segmentation Fault Comum

| Possível Causa | Verificação |
|---|---|
| Null pointer dereference | `(gdb) p ptr` |
| Array out of bounds | `(gdb) p vec.size()` |
| Use-after-free | Valgrind |
| Stack overflow | `ulimit -s` |
| Division by zero | `p denominator` |

---

## Recursos de Diagnóstico

### Comandos Úteis

```bash
# Ver portas abertas
netstat -tlnp | grep 8080
ss -tlnp | grep LISTEN

# Ver processos
ps aux | grep webserv
pgrep webserv

# Killare processo
kill -9 $(pgrep webserv)

# Testar conectividade
curl -v http://localhost:8080/
telnet localhost 8080
nc -vz localhost 8080

# Logs do system
tail -f /var/log/syslog | grep webserv
journalctl -u webserv -f

# Limpar recursos
rm -rf www/uploads/*
lsof -p $(pgrep webserv) | wc -l
```

### Ferramentas Recomendadas

```bash
# Instalar
sudo apt-get install valgrind gdb strace tcpdump apache2-utils

# Via brew (macOS)
brew install valgrind gdb tcpdump
```

### Arquivo .cgdbinit para bons defaults

```bash
cat > ~/.cgdbrc << 'EOF'
set arrowstyle short
set syntax on
set showline on
EOF
```

---

## Checklist Final de Debug

Quando o projeto não funciona:

- [ ] Compilou sem erros/warnings?
- [ ] Servidor inicia sem crash?
- [ ] `curl http://localhost:8080/` funciona?
- [ ] Arquivo index.html é servido?
- [ ] GET de arquivo .html funciona?
- [ ] POST de upload funciona?
- [ ] CGI (.py/.php) executa?
- [ ] Múltiplas conexões simultâneas?
- [ ] Keep-Alive funciona?
- [ ] Sem memory leaks (Valgrind)?
- [ ] Sem file descriptor leaks?
- [ ] Responsivo com 1000+ conexões?

Se não, voltar ao componente específico e debugar com técnicas acima.
