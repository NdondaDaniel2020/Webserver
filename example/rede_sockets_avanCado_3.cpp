// htons, htonl, ntohs, ntohl

// Converter ordem de bytes (host ↔ rede).

#include <iostream>
#include <arpa/inet.h>
#include <cstdio>

int main() {
    unsigned short port = 8080;
    unsigned long ip = 0xC0A80001; // 192.168.0.1

    std::cout << "htons(8080): " << htons(port) << std::endl;
    std::cout << "ntohs(htons(8080)): " << ntohs(htons(port)) << std::endl;

    std::cout << "htonl(192.168.0.1): " << htonl(ip) << std::endl;
    std::cout << "ntohl(htonl(...)): " << ntohl(htonl(ip)) << std::endl;

    return 0;
}
