// 🧩 PROCESSOS
// waitpid (esperar um processo específico)

#include <iostream>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    pid_t pid = fork();
    if (pid == 0) {
        std::cout << "Filho executando...\n";
        sleep(2);
        return 42;
    } else {
        int status;
        waitpid(pid, &status, 0);
        std::cout << "Filho terminou com código " << WEXITSTATUS(status) << std::endl;
    }
    return 0;
}
