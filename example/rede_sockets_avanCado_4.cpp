// getprotobyname

// Obter número de protocolo (ex: TCP, UDP)

#include <iostream>
#include <netdb.h>
#include <cstdio>

int main() {
    struct protoent *proto = getprotobyname("tcp");
    if (proto)
        std::cout << "Protocolo TCP: número " << proto->p_proto << std::endl;
    else
        perror("getprotobyname");
    return 0;
}
