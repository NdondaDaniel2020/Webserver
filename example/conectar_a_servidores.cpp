// 📡 5. Resolver nomes e conectar a servidores (com getaddrinfo, connect, freeaddrinfo)
// ➤ Exemplo: conectar-se a example.com na porta 80

#include <iostream>
#include <cstring>
#include <netdb.h>
#include <unistd.h>
#include <cstdio>

int main() {
    struct addrinfo hints, *res;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int err = getaddrinfo("example.com", "80", &hints, &res);
    if (err != 0) {
        std::cerr << "Erro: " << gai_strerror(err) << std::endl;
        return 1;
    }

    int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd < 0) { perror("socket"); return 1; }

    if (connect(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
        perror("connect");
        return 1;
    }

    const char *req = "GET / HTTP/1.0\r\nHost: example.com\r\n\r\n";
    write(sockfd, req, std::strlen(req));

    char buf[1024];
    int n = read(sockfd, buf, sizeof(buf)-1);
    buf[n] = '\0';
    std::cout << buf << std::endl;

    close(sockfd);
    freeaddrinfo(res);
    return 0;
}
