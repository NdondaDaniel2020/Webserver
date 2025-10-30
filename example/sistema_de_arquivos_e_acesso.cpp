// 🧱 SISTEMA DE ARQUIVOS E ACESSO
// 🧩 chdir, getcwd

// Mudar de diretório e verificar onde estamos.

#include <iostream>
#include <unistd.h>
#include <limits.h>
#include <cstdio>

int main() {
    char cwd[PATH_MAX];
    getcwd(cwd, sizeof(cwd));
    std::cout << "Diretório atual: " << cwd << std::endl;

    if (chdir("/tmp") == 0) {
        getcwd(cwd, sizeof(cwd));
        std::cout << "Agora em: " << cwd << std::endl;
    } else {
        perror("chdir");
    }

    return 0;
}
