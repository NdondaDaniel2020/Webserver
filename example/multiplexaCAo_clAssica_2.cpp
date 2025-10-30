// poll

// Versão mais moderna de select.

#include <iostream>
#include <poll.h>
#include <unistd.h>
#include <cstdio>

int main() {
    struct pollfd pfd;
    pfd.fd = STDIN_FILENO;
    pfd.events = POLLIN;

    std::cout << "Digite algo (ou espere 3s)...\n";
    int r = poll(&pfd, 1, 3000);
    if (r > 0)
        std::cout << "Entrada disponível!\n";
    else if (r == 0)
        std::cout << "Timeout!\n";
    else
        perror("poll");
    return 0;
}
