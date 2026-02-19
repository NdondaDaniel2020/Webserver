// 📁 ENVIAR ARQUIVO VIA SOCKET
//
// FUNÇÕES UTILIZADAS:
// - socket()     : Cria socket
// - connect()    : Conecta ao servidor (cliente)
// - open()       : Abre arquivo para leitura
// - read()       : Lê dados do arquivo
// - send()       : Envia dados pelo socket
// - close()      : Fecha arquivo e socket
//
// Este exemplo demonstra o envio de um arquivo completo via socket

#include <iostream>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>

#define BUFFER_SIZE 4096

// SERVIDOR: Recebe arquivo
void servidor_receber_arquivo(int porta) {
    // Criar socket servidor
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); return; }

    // Configurar endereço
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(porta);

    // Bind e Listen
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        return;
    }
    listen(server_fd, 5);
    std::cout << "Servidor aguardando arquivo na porta " << porta << "..." << std::endl;

    // Aceitar conexão
    int client_fd = accept(server_fd, NULL, NULL);
    if (client_fd < 0) { perror("accept"); return; }

    // Receber tamanho do arquivo primeiro
    long long file_size;
    recv(client_fd, &file_size, sizeof(file_size), 0);
    std::cout << "Tamanho do arquivo: " << file_size << " bytes" << std::endl;

    // Abrir arquivo para escrita
    int fd = open("arquivo_recebido.dat", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) { perror("open"); close(client_fd); return; }

    // Receber e escrever dados
    char buffer[BUFFER_SIZE];
    long long total_recebido = 0;
    int bytes_recebidos;

    while (total_recebido < file_size) {
        bytes_recebidos = recv(client_fd, buffer, BUFFER_SIZE, 0);
        if (bytes_recebidos <= 0) break;

        write(fd, buffer, bytes_recebidos);
        total_recebido += bytes_recebidos;
        
        // Progresso
        std::cout << "\rRecebido: " << total_recebido << "/" << file_size 
                  << " (" << (total_recebido * 100 / file_size) << "%)" << std::flush;
    }
    std::cout << std::endl << "Arquivo recebido com sucesso!" << std::endl;

    close(fd);
    close(client_fd);
    close(server_fd);
}

// CLIENTE: Envia arquivo
void cliente_enviar_arquivo(const char* arquivo, const char* host, int porta) {
    // Criar socket cliente
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) { perror("socket"); return; }

    // Configurar endereço do servidor
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(porta);
    inet_pton(AF_INET, host, &addr.sin_addr);

    // Conectar ao servidor
    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sockfd);
        return;
    }
    std::cout << "Conectado ao servidor!" << std::endl;

    // Abrir arquivo para leitura
    int fd = open(arquivo, O_RDONLY);
    if (fd < 0) {
        perror("open");
        close(sockfd);
        return;
    }

    // Obter tamanho do arquivo usando stat()
    struct stat st;
    fstat(fd, &st);
    long long file_size = st.st_size;
    
    // Enviar tamanho do arquivo primeiro
    send(sockfd, &file_size, sizeof(file_size), 0);
    std::cout << "Enviando arquivo de " << file_size << " bytes..." << std::endl;

    // Ler e enviar arquivo em blocos
    char buffer[BUFFER_SIZE];
    int bytes_lidos;
    long long total_enviado = 0;

    while ((bytes_lidos = read(fd, buffer, BUFFER_SIZE)) > 0) {
        int bytes_enviados = send(sockfd, buffer, bytes_lidos, 0);
        if (bytes_enviados < 0) {
            perror("send");
            break;
        }
        total_enviado += bytes_enviados;
        
        // Progresso
        std::cout << "\rEnviado: " << total_enviado << "/" << file_size 
                  << " (" << (total_enviado * 100 / file_size) << "%)" << std::flush;
    }
    std::cout << std::endl << "Arquivo enviado com sucesso!" << std::endl;

    close(fd);
    close(sockfd);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Uso:" << std::endl;
        std::cout << "  Servidor: " << argv[0] << " -s [porta]" << std::endl;
        std::cout << "  Cliente:  " << argv[0] << " -c <arquivo> <host> <porta>" << std::endl;
        return 1;
    }

    if (std::strcmp(argv[1], "-s") == 0) {
        int porta = (argc > 2) ? std::atoi(argv[2]) : 8080;
        servidor_receber_arquivo(porta);
    } else if (std::strcmp(argv[1], "-c") == 0 && argc >= 5) {
        cliente_enviar_arquivo(argv[2], argv[3], std::atoi(argv[4]));
    } else {
        std::cout << "Argumentos inválidos!" << std::endl;
        return 1;
    }

    return 0;
}
