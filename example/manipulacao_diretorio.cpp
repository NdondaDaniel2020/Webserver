// 📂 4. Manipulação de diretórios com opendir, readdir, closedir
// ➤ Exemplo: listar conteúdo de um diretório

#include <iostream>
#include <dirent.h>
#include <cstring>
#include <cstdio>

int main() {
    DIR *dir = opendir(".");
    if (!dir) { perror("opendir"); return 1; }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        std::cout << entry->d_name << std::endl;
    }

    closedir(dir);
    return 0;
}
