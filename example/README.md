# 🧩 1. Processos e Execução
| Função                               | O que faz                                                  | Para que serve                                                             |
| ------------------------------------ | ---------------------------------------------------------- | -------------------------------------------------------------------------- |
| **`fork()`**                         | Cria um novo processo (filho) duplicando o processo atual. | Usado para executar tarefas em paralelo ou criar subprocessos.             |
| **`execve(path, argv, envp)`**       | Substitui o processo atual por outro programa.             | Executa outro binário no lugar do processo atual (ex: `ls`, `bash`, etc.). |
| **`waitpid(pid, &status, options)`** | Espera que um processo filho termine.                      | Sincroniza o pai com o fim de um filho e obtém o código de saída.          |
| **`kill(pid, signal)`**              | Envia um sinal para um processo.                           | Serve para terminar, pausar ou acordar processos.                          |
| **`signal(sig, handler)`**           | Define uma função que lida com sinais.                     | Permite reagir a `SIGINT`, `SIGTERM`, etc.                                 |
| **`_exit(status)`**                  | Encerra imediatamente o processo (sem limpar buffers).     | Usado em filhos após `fork()` quando não se quer herdar nada do pai.       |
# 🔄 2. Comunicação entre processos
| Função                                          | O que faz                                        | Para que serve                                             |         |
| ----------------------------------------------- | ------------------------------------------------ | ---------------------------------------------------------- | ------- |
| **`pipe(fd[2])`**                               | Cria um canal unidirecional (leitura e escrita). | Permite comunicação entre pai e filho (`ls                 | grep`). |
| **`dup(oldfd)`**                                | Duplica um descritor de arquivo.                 | Copia descritor (ex: STDOUT) para outro número disponível. |         |
| **`dup2(oldfd, newfd)`**                        | Duplica descritor para um número específico.     | Redireciona saída padrão para um arquivo ou pipe.          |         |
| **`socketpair(domain, type, protocol, sv[2])`** | Cria dois sockets conectados entre si.           | Comunicação bidirecional local (sem rede).                 |         |
# 🌐 3. Rede (Sockets)
| Função                                                   | O que faz                                         | Para que serve                                       |
| -------------------------------------------------------- | ------------------------------------------------- | ---------------------------------------------------- |
| **`socket(domain, type, protocol)`**                     | Cria um ponto de comunicação de rede.             | Cria um socket TCP, UDP, etc.                        |
| **`bind(sockfd, addr, len)`**                            | Associa o socket a um IP/porta.                   | Configura servidor para ouvir numa porta específica. |
| **`listen(sockfd, backlog)`**                            | Coloca o socket em modo servidor.                 | Começa a escutar conexões TCP.                       |
| **`accept(sockfd, addr, len)`**                          | Aceita uma nova conexão.                          | Cria um novo socket para conversar com o cliente.    |
| **`connect(sockfd, addr, len)`**                         | Conecta o socket a um servidor remoto.            | Usado por clientes para se conectar a servidores.    |
| **`send(sockfd, buf, len, flags)`**                      | Envia dados por um socket.                        | Envia mensagens para outro lado da conexão.          |
| **`recv(sockfd, buf, len, flags)`**                      | Recebe dados de um socket.                        | Lê mensagens recebidas.                              |
| **`setsockopt(sockfd, level, optname, optval, optlen)`** | Ajusta opções do socket.                          | Ex: permitir reusar porta (`SO_REUSEADDR`).          |
| **`getsockname(sockfd, addr, len)`**                     | Obtém IP/porta local do socket.                   | Verifica em que porta o socket está ligado.          |
| **`getprotobyname("tcp")`**                              | Obtém o número do protocolo pelo nome.            | Usado para saber o número de protocolo (TCP/UDP).    |
| **`getaddrinfo(host, port, hints, &res)`**               | Resolve nomes de host e serviço em endereços IP.  | DNS moderno — substitui `gethostbyname`.             |
| **`freeaddrinfo(res)`**                                  | Liberta a memória alocada por `getaddrinfo`.      | Sempre chamar após usar o resultado.                 |
| **`htons(x)` / `htonl(x)`**                              | Converte valores de **host → rede** (big-endian). | Garante compatibilidade entre máquinas diferentes.   |
| **`ntohs(x)` / `ntohl(x)`**                              | Converte valores de **rede → host**.              | Reverte a conversão feita com `hton*`.               |
# ⚙️ 4. Entrada/Saída e Arquivos
| Função                        | O que faz                            | Para que serve                               |
| ----------------------------- | ------------------------------------ | -------------------------------------------- |
| **`open(path, flags, mode)`** | Abre (ou cria) um arquivo.           | Retorna um descritor para leitura/escrita.   |
| **`read(fd, buf, count)`**    | Lê dados de um arquivo ou socket.    | Pega bytes de um descritor.                  |
| **`write(fd, buf, count)`**   | Escreve dados num arquivo ou socket. | Envia bytes para o descritor.                |
| **`close(fd)`**               | Fecha o descritor.                   | Libera o recurso do kernel.                  |
| **`access(path, mode)`**      | Testa permissões de acesso.          | Verifica se é legível, gravável, executável. |
| **`stat(path, &st)`**         | Obtém informações sobre arquivo.     | Tamanho, tipo, permissões, etc.              |
| **`chdir(path)`**             | Muda o diretório de trabalho.        | Equivale a `cd` no terminal.                 |
# 📂 5. Diretórios
| Função               | O que faz                                    | Para que serve                            |
| -------------------- | -------------------------------------------- | ----------------------------------------- |
| **`opendir(path)`**  | Abre um diretório.                           | Prepara para listar arquivos dentro dele. |
| **`readdir(dirp)`**  | Lê uma entrada (arquivo/pasta) do diretório. | Itera sobre o conteúdo.                   |
| **`closedir(dirp)`** | Fecha o diretório aberto.                    | Libera recurso do sistema.                |
# 🧠 6. Multiplexação (Esperar eventos em vários descritores)
| Função                                                              | O que faz                                            | Para que serve                                         |
| ------------------------------------------------------------------- | ---------------------------------------------------- | ------------------------------------------------------ |
| **`select(nfds, &readfds, &writefds, &exceptfds, &timeout)`**       | Espera atividade em múltiplos descritores.           | Ex: esperar dados do teclado e socket ao mesmo tempo.  |
| **`poll(pfd[], nfds, timeout)`**                                    | Similar ao `select`, mas mais eficiente e escalável. | Espera por leitura/escrita sem limites fixos.          |
| **`epoll_create(size)`**                                            | Cria uma instância de epoll (Linux).                 | Base de um sistema eficiente de monitoramento de I/O.  |
| **`epoll_ctl(epfd, op, fd, event)`**                                | Adiciona/remove/atualiza descritores no epoll.       | Controla quais descritores monitorar.                  |
| **`epoll_wait(epfd, events[], maxevents, timeout)`**                | Espera por eventos em descritores.                   | Desperta quando algo está pronto.                      |
| **`kqueue()`**                                                      | Cria uma fila de eventos (BSD/macOS).                | Similar ao `epoll`, mas em outro sistema.              |
| **`kevent(kq, changelist, nchanges, eventlist, nevents, timeout)`** | Adiciona/espera eventos em `kqueue`.                 | Permite detectar leitura, escrita, sinais, timers etc. |
# ⚡ 7. Erros e mensagens
| Função                   | O que faz                                           | Para que serve                                         |
| ------------------------ | --------------------------------------------------- | ------------------------------------------------------ |
| **`errno`**              | Variável global que guarda o último código de erro. | Usada após chamadas que falham (`open`, `read`, etc.). |
| **`strerror(errno)`**    | Retorna uma string com a descrição do erro.         | Mostra erro legível (“Permission denied”, etc.).       |
| **`gai_strerror(code)`** | Retorna mensagem de erro de funções `getaddrinfo`.  | Exibe erro DNS legível.                                |
# 🔚 8. Controle de processos e comunicação
| Função        | O que faz                                   | Para que serve                         |
| ------------- | ------------------------------------------- | -------------------------------------- |
| **`waitpid`** | Espera fim de um processo filho específico. | Obter retorno de execução.             |
| **`kill`**    | Envia sinal a processo.                     | Finalizar ou controlar outro processo. |
| **`signal`**  | Define manipulador para um sinal.           | Ex: capturar `SIGINT` (Ctrl+C).        |
# ✅ Resumo funcional rápido
| Categoria                                   | Uso principal                                   |
| ------------------------------------------- | ----------------------------------------------- |
| **Processos (`fork`, `execve`, `waitpid`)** | Criar e gerenciar subprocessos                  |
| **Pipes / `dup` / `dup2`**                  | Comunicação e redirecionamento de entrada/saída |
| **Sockets / Rede**                          | Comunicação entre computadores                  |
| **E/S (`read`, `write`, `open`, `close`)**  | Manipulação de arquivos e dados                 |
| **Diretórios (`opendir`, `readdir`)**       | Percorrer o sistema de arquivos                 |
| **Eventos (`select`, `poll`, `epoll`)**     | Esperar vários eventos simultaneamente          |
| **Sinais (`signal`, `kill`)**               | Controle de processos e interrupções            |
| **Erros (`errno`, `strerror`)**             | Diagnóstico de falhas                           |

