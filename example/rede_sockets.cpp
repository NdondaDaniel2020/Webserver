// 🌐 2. Rede (Sockets) — servidor TCP simples com socket, bind, listen, accept, send, recv
// ➤ Exemplo: ecoar o que o cliente envia

#include <iostream>
#include <cstring>
#include <unistd.h>
#include <netinet/in.h>
#include <cstdio>

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); return 1; }

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(8080); // Porta 8080

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }

    listen(server_fd, 1);
    std::cout << "Servidor ouvindo na porta 8080..." << std::endl;

    int client_fd = accept(server_fd, NULL, NULL);
    if (client_fd < 0) { perror("accept"); return 1; }

    char buffer[1024];
    int n = recv(client_fd, buffer, sizeof(buffer)-1, 0);
    buffer[n] = '\0';
    std::cout << "Recebido: " << buffer << std::endl;

    send(client_fd, buffer, n, 0); // ecoa
    close(client_fd);
    close(server_fd);
    return 0;
}

