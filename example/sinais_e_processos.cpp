// ⚔️ 7. Sinais e processos: fork, kill, signal, waitpid
// ➤ Exemplo: matar um filho após 3 segundos

#include <iostream>
#include <csignal>
#include <unistd.h>
#include <sys/wait.h>
#include <cstdio>

void handler(int signum) {
    std::cout << "Filho recebeu sinal " << signum << std::endl;
    _exit(0);
}

int main() {
    pid_t pid = fork();
    if (pid == 0) {
        signal(SIGTERM, handler);
        while (1) { pause(); }
    } else {
        sleep(3);
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
        std::cout << "Filho terminado.\n";
    }
    return 0;
}
