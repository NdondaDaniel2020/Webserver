// 💥 ERROS E MENSAGENS
// errno, strerror

// Mostrar mensagens de erro humanas.

#include <iostream>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <cstdio>

int main() {
    int fd = open("arquivo_inexistente.txt", O_RDONLY);
    if (fd < 0) {
        std::cerr << "Erro (" << errno << "): "
                    << strerror(errno) << std::endl;
    }
    return 0;
}
