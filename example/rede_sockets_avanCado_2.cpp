// setsockopt e getsockname

// Ativar SO_REUSEADDR e obter o nome do socket (IP/porta).

#include <iostream>
#include <cstring>
#include <unistd.h>
#include <netinet/in.h>
#include <cstdio>

int main() {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(8081);

    bind(s, (struct sockaddr*)&addr, sizeof(addr));

    socklen_t len = sizeof(addr);
    getsockname(s, (struct sockaddr*)&addr, &len);
    std::cout << "Socket vinculado na porta "
            << ntohs(addr.sin_port) << std::endl;

    close(s);
    return 0;
}

