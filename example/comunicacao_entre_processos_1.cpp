
// 🧩 1. Comunicação entre processos com pipe + fork + dup2 + execve
// ➤ Exemplo: Simular ls | wc -l

#include <unistd.h>
#include <sys/wait.h>
#include <cstdio>
#include <iostream>

int main() {
    int fd[2];
    pipe(fd);

    pid_t pid = fork();
    if (pid == 0) {
        // Processo filho: executa "ls"
        dup2(fd[1], STDOUT_FILENO); // Redireciona saída padrão para o pipe
        close(fd[0]);
        close(fd[1]);
        char *args[] = {(char*)"ls", (char*)NULL};
        execve("/bin/ls", args, NULL);
        perror("execve");
        return 1;
    } else {
        // Processo pai: executa "wc -l"
        dup2(fd[0], STDIN_FILENO);
        close(fd[0]);
        close(fd[1]);
        char *args[] = {(char*)"wc", (char*)"-l", (char*)NULL};
        execve("/usr/bin/wc", args, NULL);
        perror("execve");
        return 1;
    }
}
