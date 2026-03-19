/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nmatondo <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/10/30 13:40:20 by nmatondo          #+#    #+#             */
/*   Updated: 2026/03/19 11:28:54 by nmatondo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"

Server::Server(const ConfigParser &config)
    : port_count(config.getServerCount()),
      config(config),
      TIMEOUT_SECONDS(120)
{
    // ---------- Portas ----------
    this->ports = new int[this->port_count];
    this->interface = new std::string[this->port_count];
    for (int i = 0; i < this->port_count; i++)
    {
        this->ports[i] = config.getServerConfig(i).port;
        this->interface[i] = config.getServerConfig(i).interface;
    }

    // ---------- Criar epoll ----------
    this->epoll_fd = epoll_create(1);
    if (this->epoll_fd < 0)
        perror("epoll_create");

    // ---------- Criar sockets servidores ----------
    this->servers = new int[this->port_count];
    for (int i = 0; i < this->port_count; i++)
    {
        this->servers[i] = createServerSocket(this->interface[i], this->ports[i]);
        if (this->servers[i] < 0)
            return;

        epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = this->servers[i];

        epoll_ctl(this->epoll_fd, EPOLL_CTL_ADD, this->servers[i], &ev);
    }
}

Server::~Server()
{
    // Limpar todos os clientes
    for (std::map<int, Client *>::iterator it = clients.begin(); it != clients.end(); ++it)
    {
        delete it->second;
    }
    clients.clear();
    delete[] this->ports;
    delete[] this->servers;
    delete[] this->interface;
}

void Server::start()
{
    std::cout << "Servidor iniciado. Aguardando conexões..." << std::endl;

    while (true)
    {
        checkTimeout();

        int n = epoll_wait(this->epoll_fd, this->events, 64, 1000);
        if (n < 0 && errno != EINTR)
        {
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < n; i++)
        {
            int fd = this->events[i].data.fd;

            if (isServerSocket(fd))
            {
                newConnection(fd);
                continue;
            }

            std::map<int, Client*>::iterator it = clients.find(fd);
            if (it != clients.end())
            {
                Client* client = it->second;

                if (events[i].events & (EPOLLERR | EPOLLHUP))
                {
                    closeClient(fd);
                    continue;
                }

                if (events[i].events & EPOLLIN)
                    handleClientData(fd);

                // Revalidar iterador após handleClientData (pode ter fechado o cliente)
                it = clients.find(fd);
                if (it == clients.end())
                    continue;
                client = it->second;

                if (events[i].events & EPOLLOUT)
                {
                    if (client->getState() == Client::SENDING_RESPONSE
                        && client->hasDataToSend())
                    {
                        bool finished = client->sendData();
                        if (finished)
                        {
                            if (client->isKeepAlive())
                                client->reset();
                            else
                                closeClient(fd);
                        }
                        else
                            closeClient(fd);
                    }
                }
            }
            else
            {
                // fd pertence a um pipe CGI
                std::map<int, Client*>::iterator cit = cgi_fd_map.find(fd);
                if (cit != cgi_fd_map.end())
                {
                    Client* c = cit->second;
                    if (events[i].events & (EPOLLIN | EPOLLERR | EPOLLHUP))
                        c->handleCgiStdoutReadable(epoll_fd);
                    else if (events[i].events & EPOLLOUT)
                        c->handleCgiStdinWritable(epoll_fd);
                }
            }
        }
    }
}

void Server::newConnection(int fd)
{
    int client_fd = accept(fd, NULL, NULL);
    if (client_fd < 0)
    {
        perror("accept");
        return;
    }

    fcntl(client_fd, F_SETFL, O_NONBLOCK);

    std::cout << "[+] Cliente conectado fd=" << client_fd << std::endl;

    Client *client = new Client(client_fd, &this->config);
    this->clients[client_fd] = client;
    this->customer_origin[client_fd] = fd;

    epoll_event cev;
    cev.events = EPOLLIN | EPOLLOUT;
    cev.data.fd = client_fd;

    epoll_ctl(this->epoll_fd, EPOLL_CTL_ADD, client_fd, &cev);
}

void Server::handleClientData(int fd)
{
    std::map<int, Client*>::iterator it = this->clients.find(fd);
    if (it == this->clients.end())
    {
        std::cerr << "[ERRO] Cliente fd=" << fd << " não encontrado" << std::endl;
        return;
    }

    Client* client = it->second;

    if (client->getState() == Client::READING_HEADERS ||
        client->getState() == Client::READING_BODY)
    {
        char buf[4096];
        int r = read(fd, buf, sizeof(buf));

        if (r <= 0)
        {
            std::cout << "[-] Cliente desconectado fd=" << fd << std::endl;
            closeClient(fd);
            return;
        }

        client->appendRecvData(buf, r);

        if (client->isRequestComplete())
        {
            for (int i = 0; i < this->port_count; i++)
            {
                if (this->customer_origin[fd] == this->servers[i])
                {
                    client->processRequest(this->config.getServerConfig(i), this->epoll_fd);
                    break;
                }
            }

            // Registar pipes CGI no mapa separado, nunca em clients
            if (client->isCgiActive())
            {
                int out_fd = client->getCgiOutFd();
                int in_fd  = client->getCgiInFd();
                if (out_fd >= 0) cgi_fd_map[out_fd] = client;
                if (in_fd  >= 0) cgi_fd_map[in_fd]  = client;
            }
        }
    }

    if (client->getState() == Client::SENDING_RESPONSE)
    {
        if (client->hasDataToSend())
        {
            bool finished = client->sendData();
            if (finished)
            {
                if (client->isKeepAlive())
                    client->reset();
                else
                    closeClient(fd);
            }
            else
                closeClient(fd);
        }
    }
}

void Server::closeClient(int fd)
{
    std::map<int, Client*>::iterator it = this->clients.find(fd);
    if (it == this->clients.end())
        return;

    Client* client = it->second;

    // Guardar fds dos pipes ANTES do cleanup os fechar
    int cgi_out = -1;
    int cgi_in  = -1;
    if (client->isCgiActive())
    {
        cgi_out = client->getCgiOutFd();
        cgi_in  = client->getCgiInFd();
    }

    // Cleanup: mata processo, fecha e anula pipes
    client->cleanupCgiIfActive(this->epoll_fd);

    // Remover pipes do mapa separado
    if (cgi_out >= 0) cgi_fd_map.erase(cgi_out);
    if (cgi_in  >= 0) cgi_fd_map.erase(cgi_in);

    delete client;
    this->clients.erase(it);

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

void Server::checkTimeout()
{
    time_t now = time(NULL);

    // Iterar sobre todos os clientes
    for (std::map<int, Client *>::iterator it = clients.begin(); it != clients.end();)
    {
        Client *client = it->second;
        int client_fd = it->first;

        // Verificar se o cliente está inativo por mais tempo que TIMEOUT_SECONDS
        time_t time_inactive = now - client->getLastActivity();

        if (time_inactive > TIMEOUT_SECONDS)
        {
            std::cout << "[TIMEOUT] Cliente " << client_fd << " inativo por "
                      << time_inactive << " segundos (limite: "
                      << TIMEOUT_SECONDS << ")" << std::endl;

            // Avança o iterador ANTES de fechar o cliente (fechar remove o elemento)
            ++it;
            closeClient(client_fd);
        }
        else
            ++it;
    }
}

int Server::createServerSocket(const std::string &interface, int port)
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("socket");
        return -1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(ipToHex(interface));
    addr.sin_port = htons(port);

    if (bind(server_fd, (sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        close(server_fd);
        return -1;
    }

    if (listen(server_fd, 10) < 0)
    {
        perror("listen");
        close(server_fd);
        return -1;
    }

    std::cout << "Servidor ouvindo na porta " << port
              << " http://" << interface << ":" << port << std::endl;
    return server_fd;
}
