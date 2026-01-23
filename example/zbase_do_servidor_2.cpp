#include <iostream>
#include <cstring>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/epoll.h>

int create_server_socket(int port) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) { perror("socket"); return -1; }

    int opt = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(s, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(s);
        return -1;
    }

    if (listen(s, 10) < 0) {
        perror("listen");
        close(s);
        return -1;
    }

    std::cout << "Servidor ouvindo na porta " << port << std::endl;
    return s;
}

int main() {

    // ---------- Portas ----------
    int ports[] = {8080, 8000, 8888};
    const int PORT_COUNT = 3;

    // ---------- Criar epoll ----------
    int epfd = epoll_create(1);
    if (epfd < 0) { perror("epoll_create"); return 1; }

    // ---------- Criar sockets servidores ----------
    int servers[PORT_COUNT];

    for (int i = 0; i < PORT_COUNT; i++) {
        servers[i] = create_server_socket(ports[i]);
        if (servers[i] < 0) return 1;

        epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = servers[i];

        epoll_ctl(epfd, EPOLL_CTL_ADD, servers[i], &ev);
    }

    epoll_event events[64];

    // ---------- Loop principal ----------
    while (true) {
        int n = epoll_wait(epfd, events, 64, -1);
        if (n < 0) { perror("epoll_wait"); break; }

        for (int i = 0; i < n; i++) {
            int fd = events[i].data.fd;

            bool is_server = false;

            // Verificar se é socket servidor
            for (int j = 0; j < PORT_COUNT; j++) {
                if (fd == servers[j]) {
                    is_server = true;
                    break;
                }
            }

            // ---------- Nova conexão ----------
            if (is_server) {
                int client_fd = accept(fd, NULL, NULL);
                if (client_fd < 0) { perror("accept"); continue; }

                std::cout << "[+] Cliente conectado fd=" << client_fd << std::endl;

                epoll_event cev;
                cev.events = EPOLLIN;
                cev.data.fd = client_fd;

                epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &cev);
            }

            // ---------- Dados de cliente ----------
            else {
                char buf[1024];
                int r = read(fd, buf, sizeof(buf)-1);

                if (r <= 0) {
                    std::cout << "[-] Cliente desconectado fd=" << fd << std::endl;
                    close(fd);
                    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                    continue;
                }

                buf[r] = '\0';
                std::cout << "[fd " << fd << "] " << buf;

                // Ecoar
                write(fd, buf, r);
            }
        }
    }

    // ---------- Fechar tudo ----------
    for (int i = 0; i < PORT_COUNT; i++)
        close(servers[i]);

    close(epfd);
    return 0;
}
