// 🔍 6. Obter informações de ficheiros com stat, access, open, read, close
// ➤ Exemplo: ler e mostrar o conteúdo de um arquivo se existir

#include <iostream>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstdio>

int main() {
    const char *filename = "teste.txt";
    if (access(filename, R_OK) != 0) {
        perror("access");
        return 1;
    }

    struct stat st;
    if (stat(filename, &st) == 0)
        std::cout << "Tamanho: " << st.st_size << " bytes\n";

    int fd = open(filename, O_RDONLY);
    if (fd < 0) { perror("open"); return 1; }

    char buf[128];
    int n;
    while ((n = read(fd, buf, sizeof(buf)-1)) > 0) {
        buf[n] = '\0';
        std::cout << buf;
    }

    close(fd);
    return 0;
}
