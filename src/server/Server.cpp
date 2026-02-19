/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nmatondo <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/10/30 13:40:20 by nmatondo          #+#    #+#             */
/*   Updated: 2026/02/19 13:03:20 by nmatondo         ###   ########.fr       */
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
    // Limpar todos os clientes
    for (std::map<int, Client*>::iterator it = clients.begin(); it != clients.end(); ++it)
    {
        delete it->second;
    }
    clients.clear();
    
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
    std::cout << "Servidor iniciado. Aguardando conexões..." << std::endl;
    
    while (true)
    {
        int n = epoll_wait(this->epoll_fd, this->events, 64, -1);
        if (n < 0) { perror("epoll_wait"); break; }

        for (int i = 0; i < n; i++)
        {
            int fd = this->events[i].data.fd;

            if (isServerSocket(fd))
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

    // Criar objeto Client
    Client* client = new Client(client_fd, &this->config);
    this->clients[client_fd] = client;

    // Adicionar ao epoll
    epoll_event cev;
    cev.events = EPOLLIN | EPOLLOUT;  // Monitora leitura e escrita
    cev.data.fd = client_fd;

    epoll_ctl(this->epoll_fd, EPOLL_CTL_ADD, client_fd, &cev);
}

void Server::handleClientData(int fd)
{
    // Buscar cliente
    std::map<int, Client*>::iterator it = this->clients.find(fd);
    if (it == this->clients.end())
    {
        std::cerr << "[ERRO] Cliente fd=" << fd << " não encontrado" << std::endl;
        closeClient(fd);
        return;
    }

    Client* client = it->second;
    
    // ========== LEITURA ==========
    if (client->getState() == Client::READING_HEADERS || 
        client->getState() == Client::READING_BODY)
    {
        char buf[4096];
        int r = read(fd, buf, sizeof(buf));
        
        if (r <= 0)
        {
            // Cliente desconectou ou erro
            std::cout << "[-] Cliente desconectado fd=" << fd << std::endl;
            closeClient(fd);
            return;
        }
        
        // Adicionar dados ao buffer do cliente
        client->appendRecvData(buf, r);
        
        // Verificar se requisição está completa
        if (client->isRequestComplete())
        {
            // Processar requisição
            client->processRequest(this->config.getServerConfig(0));
        }
    }
    
    // ========== ESCRITA ==========
    if (client->getState() == Client::SENDING_RESPONSE)
    {
        if (client->hasDataToSend())
        {
            bool finished = client->sendData();
            
            if (finished)
            {
                // Resposta enviada completamente
                if (client->isKeepAlive())
                {
                    // Reset para próxima requisição
                    client->reset();
                }
                else
                {
                    // Fechar conexão
                    closeClient(fd);
                }
            }
        }
    }
}

void Server::closeClient(int fd)
{
    std::map<int, Client*>::iterator it = this->clients.find(fd);
    if (it != this->clients.end())
    {
        delete it->second;
        this->clients.erase(it);
    }
    
    epoll_ctl(this->epoll_fd, EPOLL_CTL_DEL, fd, NULL);
    close(fd);
    std::cout << "[-] Cliente fd=" << fd << " fechado" << std::endl;
}

bool Server::isServerSocket(int fd) const
{
    for (int i = 0; i < this->port_count; i++)
    {
        if (fd == this->servers[i])
            return true;
    }
    return false;
}
