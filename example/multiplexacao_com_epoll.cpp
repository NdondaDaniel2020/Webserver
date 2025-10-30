// ⚙️ 3. Multiplexação com epoll (Linux)
// ➤ Exemplo: monitorar STDIN e um socket com epoll

#include <iostream>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstdio>

int main() {
    int epfd = epoll_create(1);
    if (epfd < 0) { perror("epoll_create"); return 1; }

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = STDIN_FILENO;
    epoll_ctl(epfd, EPOLL_CTL_ADD, STDIN_FILENO, &ev);

    std::cout << "Digite algo (Ctrl+C para sair):\n";
    while (1) {
        struct epoll_event events[1];
        int n = epoll_wait(epfd, events, 1, -1);
        if (n > 0 && (events[0].events & EPOLLIN)) {
            char buf[256];
            int r = read(STDIN_FILENO, buf, sizeof(buf) - 1);
            if (r <= 0) break;
            buf[r] = '\0';
            std::cout << "Lido: " << buf;
        }
    }

    close(epfd);
    return 0;
}
