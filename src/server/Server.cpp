/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nmatondo <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/10/30 13:40:20 by nmatondo          #+#    #+#             */
/*   Updated: 2026/02/03 12:04:41 by nmatondo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"

Server::Server(const ConfigParser& config) : port_count(config.getServerCount()), config(config)
{
    // ---------- Portas ----------
    this->ports = new int[this->port_count];
    
    for (int i = 0; i < this->port_count; i++)
        this->ports[i] = config.getServerConfig(i).port;

    // ---------- Criar epoll ----------
    this->epoll_fd = epoll_create(1);
    if (this->epoll_fd < 0) 
        perror("epoll_create");

    // ---------- Criar sockets servidores ----------
    this->servers = new int[this->port_count];
    for (int i = 0; i < this->port_count; i++) {
        this->servers[i] = createServerSocket(this->ports[i]);
        if (this->servers[i] < 0) return;
        epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = this->servers[i];

        epoll_ctl(this->epoll_fd, EPOLL_CTL_ADD, this->servers[i], &ev);
    }
}

Server::~Server()
{
    delete[] this->ports;
    delete[] this->servers;
}

Server::Server(const Server& other) : port_count(other.port_count)
{
    this->ports = new int[this->port_count];
    std::memcpy(this->ports, other.ports, sizeof(int) * this->port_count);
    this->epoll_fd = other.epoll_fd;
    this->servers = new int[this->port_count];
    std::memcpy(this->servers, other.servers, sizeof(int) * this->port_count);
    std::memcpy(this->events, other.events, sizeof(other.events));
}

Server& Server::operator=(const Server& other)
{
    if (this != &other)
    {
        delete[] this->ports;
        delete[] this->servers;
        this->port_count = other.port_count;
        this->ports = new int[this->port_count];
        std::memcpy(this->ports, other.ports, sizeof(int) * this->port_count);
        this->epoll_fd = other.epoll_fd;
        this->servers = new int[this->port_count];
        std::memcpy(this->servers, other.servers, sizeof(int) * this->port_count);
        std::memcpy(this->events, other.events, sizeof(other.events));
    }
    return *this;
}

void Server::start()
{
    while (true)
    {
        int n = epoll_wait(this->epoll_fd, this->events, 64, -1);
        if (n < 0) { perror("epoll_wait"); break; }

        for (int i = 0; i < n; i++)
        {
            int fd = this->events[i].data.fd;

            bool is_server = false;
            // Verificar se é socket servidor
            for (int j = 0; j < this->port_count; j++)
            {
                if (fd == this->servers[j]) {
                    is_server = true;
                    break;
                }
            }

            if (is_server)
                newConnection(fd);  // ---------- Nova conexão ----------
            else
                handleClientData(fd);  // ---------- Dados de cliente ----------
        }
    }
}

void Server::stop()
{
    for (int i = 0; i < this->port_count; i++)
        close(this->servers[i]);
    close(this->epoll_fd);
}

int Server::createServerSocket(int port)
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); return -1; }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        return -1;
    }

    if (listen(server_fd, 10) < 0) {
        perror("listen");
        close(server_fd);
        return -1;
    }

    std::cout << "Servidor ouvindo na porta " << port 
              << " http://localhost:" << port << std::endl;
    return server_fd;
}

void Server::newConnection(int fd)
{
    int client_fd = accept(fd, NULL, NULL);
    if (client_fd < 0) { perror("accept"); return; }

    std::cout << "[+] Cliente conectado fd=" << client_fd << std::endl;

    epoll_event cev;
    cev.events = EPOLLIN;
    cev.data.fd = client_fd;

    epoll_ctl(this->epoll_fd, EPOLL_CTL_ADD, client_fd, &cev);
}

void Server::handleClientData(int fd)
{
    char buf[1024];
    int r = read(fd, buf, sizeof(buf)-1);

    if (r <= 0) {
        std::cout << "[-] Cliente desconectado fd=" << fd << std::endl;
        close(fd);
        epoll_ctl(this->epoll_fd, EPOLL_CTL_DEL, fd, NULL);
        return ;
    }

    buf[r] = '\0';
    std::string response = Response(buf, this->config.getServerConfig(0)).getResponseHttp();
    write(fd, response.c_str(), response.size());
    close(fd);
    epoll_ctl(this->epoll_fd, EPOLL_CTL_DEL, fd, NULL);
}
