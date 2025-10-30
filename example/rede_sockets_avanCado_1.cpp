// 📡 REDE / SOCKETS AVANÇADO
// socketpair

// Cria dois sockets conectados entre si (útil para comunicação entre processos).

#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdio>

int main() {
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
        perror("socketpair");
        return 1;
    }

    pid_t pid = fork();
    if (pid == 0) {
        close(sv[0]);
        const char *msg = "Olá do filho!";
        write(sv[1], msg, 14);
    } else {
        close(sv[1]);
        char buf[100];
        int n = read(sv[0], buf, sizeof(buf)-1);
        buf[n] = '\0';
        std::cout << "Pai recebeu: " << buf << std::endl;
    }

    return 0;
}
