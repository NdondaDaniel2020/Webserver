// ⚙️ DUPLICAÇÃO DE DESCRITORES
// dup e dup2

// Duplicar stdout para escrever em arquivo e tela ao mesmo tempo.

#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <cstdio>

int main() {
    int fd = open("saida.txt", O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) { perror("open"); return 1; }

    int saved_stdout = dup(STDOUT_FILENO);
    dup2(fd, STDOUT_FILENO); // Redireciona stdout para o arquivo

    std::cout << "Isto vai para o arquivo!\n";

    dup2(saved_stdout, STDOUT_FILENO); // Restaura stdout
    std::cout << "Isto volta para o terminal!\n";

    close(fd);
    close(saved_stdout);
    return 0;
}
