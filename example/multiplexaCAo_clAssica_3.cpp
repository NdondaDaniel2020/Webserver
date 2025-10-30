// kqueue e kevent (BSD/macOS)

// Simples monitoramento de leitura no stdin.

#include <iostream>
#include <sys/event.h>
#include <sys/time.h>
#include <unistd.h>
#include <cstdio>

int main() {
    int kq = kqueue();
    struct kevent ev;
    EV_SET(&ev, STDIN_FILENO, EVFILT_READ, EV_ADD, 0, 0, NULL);
    kevent(kq, &ev, 1, NULL, 0, NULL);

    std::cout << "Digite algo:\n";
    struct kevent out;
    kevent(kq, NULL, 0, &out, 1, NULL);
    if (out.filter == EVFILT_READ) {
        char buf[100];
        int n = read(STDIN_FILENO, buf, sizeof(buf)-1);
        buf[n] = '\0';
        std::cout << "Lido: " << buf;
    }

    close(kq);
    return 0;
}
