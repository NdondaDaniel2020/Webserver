/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nmatondo <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/10/30 13:40:20 by nmatondo          #+#    #+#             */
/*   Updated: 2025/11/04 12:58:34 by nmatondo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"


Server::Server(int port)
{
    this->server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (this->server_fd < 0)
        throw std::runtime_error("socket");

    int opt = 1;
    setsockopt(this->server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    std::memset(&this->addr, 0, sizeof(this->addr));
    this->addr.sin_family = AF_INET;
    this->addr.sin_addr.s_addr = INADDR_ANY; 
    this->addr.sin_port = htons(port);
}


Server::~Server()
{
    close(this->server_fd);
}


void Server::start()
{
    this->setupSocket();
    this->setupEpoll();
    while (true)
    {
        this->handleEvents();
    }    
}


void Server::stop()
{
    close(this->epoll_fd);
    close(this->server_fd);
}


void Server::setupSocket() // Configurar Socket
{
    if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0)
        throw std::runtime_error(create_error_message("bind"));

    if (listen(server_fd, 10) < 0)
        throw std::runtime_error(create_error_message("listen"));
    
    std::cout << "Servidor EPOLL ouvindo na porta 8080..." << std::endl;

}


void Server::setupEpoll() // Configurar Epoll
{
    this->epoll_fd = epoll_create(1);
    if (this->epoll_fd < 0)
        throw std::runtime_error(create_error_message("epoll_create"));

    // Estrutura de evento
    epoll_event ev;
    ev.events = EPOLLIN;   // queremos saber quando tiver dados para ler
    ev.data.fd = this->server_fd;

    // Monitorar o socket do servidor
    if (epoll_ctl(this->epoll_fd, EPOLL_CTL_ADD, this->server_fd, &ev) < 0)
        throw std::runtime_error(create_error_message("epoll_ctl"));
}


void Server::handleEvents() // Lidar com Eventos
{
    int n = epoll_wait(this->epoll_fd, this->list_events, 64, -1);
    if (n < 0) { perror("epoll_wait"); return; }

    for (int i = 0; i < n; i++)
    {
        if (this->list_events[i].data.fd == this->server_fd)
        {
            if (this->handleNewConnection())
                continue;
        }
        else
        {
            this->handleClientData(this->list_events[i].data.fd);
        }
    }
}


int Server::handleNewConnection() // Lidar com Nova Conexão
{
    int client_fd = accept(this->server_fd, NULL, NULL);
    if (client_fd < 0) { perror("accept"); return 1; }

    std::cout << "[+] Cliente conectado: fd=" << client_fd << std::endl;

    epoll_event client_ev;
    client_ev.events = EPOLLIN;   // queremos ler dados dele
    client_ev.data.fd = client_fd;

    epoll_ctl(this->epoll_fd, EPOLL_CTL_ADD, client_fd, &client_ev);
    return 0;
}


void Server::handleClientData(int client_fd) // Lidar com Dados do Cliente
{
    char buf[1024];
    int r = read(client_fd, buf, sizeof(buf)-1);

    // Cliente desconectou
    if (r <= 0)
    {
        std::cout << "[-] Cliente desconectado: fd=" << client_fd << std::endl;
        close(client_fd);
        epoll_ctl(this->epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
        return;
    }

    buf[r] = '\0';
    std::cout << "[fd " << client_fd << "] disse: " << buf;

    // ---------- Enviar Resposta HTTP Válida ----------
    std::string response = "HTTP/1.1 200 OK\r\n";
    response += "Content-Type: text/html; charset=UTF-8\r\n";
    response += "Content-Length: 34\r\n";
    response += "Connection: close\r\n";
    response += "\r\n";
    response += "<html><body>Hello World!</body></html>";
    write(client_fd, response.c_str(), response.size());

    // ---------- 7. Ecoar de volta ----------
    write(client_fd, buf, r);
    // close(client_fd);
}
