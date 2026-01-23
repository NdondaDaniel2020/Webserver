#include <iostream>
#include <cstring>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <fcntl.h>

int main() {
    // ---------- 1. Criar socket servidor ----------
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); return 1; }

    // Reutilizar porta rapidamente
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // ---------- 2. Endereço ----------
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY; 
    addr.sin_port = htons(8080);

    if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }

    if (listen(server_fd, 10) < 0) {
        perror("listen");
        return 1;
    }

    std::cout << "Servidor EPOLL ouvindo na porta 8080..." << std::endl;


    // ---------- 3. Criar epoll ----------
    int epfd = epoll_create(1);
    if (epfd < 0) { perror("epoll_create"); return 1; }

    // Estrutura de evento
    epoll_event ev;
    ev.events = EPOLLIN;   // queremos saber quando tiver dados para ler
    ev.data.fd = server_fd;

    // Monitorar o socket do servidor
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, server_fd, &ev) < 0) {
        perror("epoll_ctl");
        return 1;
    }

    // Buffer de eventos
    epoll_event events[64];

    // ---------- 4. Loop principal ----------
    while (true) {
        int n = epoll_wait(epfd, events, 64, -1);
        if (n < 0) { perror("epoll_wait"); break; }

        for (int i = 0; i < n; i++) {

            // ---------- 5. Novo cliente se conectando ----------
            if (events[i].data.fd == server_fd) {
                int client_fd = accept(server_fd, NULL, NULL);
                if (client_fd < 0) { perror("accept"); continue; }

                std::cout << "[+] Cliente conectado: fd=" << client_fd << std::endl;

                epoll_event client_ev;
                client_ev.events = EPOLLIN;   // queremos ler dados dele
                client_ev.data.fd = client_fd;

                epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &client_ev);
            }

            // ---------- 6. Cliente enviou dados ----------
            else {
                int client_fd = events[i].data.fd;

                char buf[1024];
                int r = read(client_fd, buf, sizeof(buf)-1);

                // Cliente desconectou
                if (r <= 0) {
                    std::cout << "[-] Cliente desconectado: fd=" << client_fd << std::endl;
                    close(client_fd);
                    epoll_ctl(epfd, EPOLL_CTL_DEL, client_fd, NULL);
                    continue;
                }

                buf[r] = '\0';
                std::cout << "[fd " << client_fd << "] disse: " << buf;

                // ---------- 7. Ecoar de volta ----------
                write(client_fd, buf, r);
            }
        }
    }

    close(epfd);
    close(server_fd);
    return 0;
}
