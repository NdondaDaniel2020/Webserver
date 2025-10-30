// select

// Esperar entrada do teclado com timeout.

#include <iostream>
#include <sys/select.h>
#include <unistd.h>
#include <cstdio>

int main() {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);

    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;

    std::cout << "Digite algo (ou espere 5s)...\n";
    int r = select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
    if (r > 0) {
        char buf[100];
        int n = read(STDIN_FILENO, buf, sizeof(buf)-1);
        buf[n] = '\0';
        std::cout << "Você digitou: " << buf;
    } else if (r == 0)
        std::cout << "Timeout!\n";
    else
        perror("select");
    return 0;
}
