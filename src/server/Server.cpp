/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nmatondo <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/10/30 13:40:20 by nmatondo          #+#    #+#             */
/*   Updated: 2026/04/03 15:14:51 by nmatondo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"

Server::Server(const ConfigParser &config)
    : ports(NULL),
      epoll_fd(-1),
      servers(NULL),
      port_count(config.getServerCount()),
      interface(NULL),
      config(config),
      TIMEOUT_SECONDS(120),
      CGI_TIMEOUT_SECONDS(60),
      shutdown_requested(false),
      cleaned_up(false)
{
    this->ports = new int[this->port_count];
    this->interface = new std::string[this->port_count];
    this->servers = new int[this->port_count];

    for (int i = 0; i < this->port_count; i++)
    {
        this->ports[i] = config.getServerConfig(i).port;
        this->interface[i] = config.getServerConfig(i).interface;
        servers[i] = -1;
    }

    this->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (this->epoll_fd < 0)
    {
        perror("epoll_create");
        cleanup();
        throw std::runtime_error("Failed to create epoll");
    }

    for (int i = 0; i < this->port_count; i++)
    {
        this->servers[i] = createServerSocket(this->interface[i], this->ports[i]);
        if (this->servers[i] < 0)
        {
            std::cerr << "[ERRO] Falha ao criar socket para porta " << this->ports[i] << std::endl;
            cleanup();
            throw std::runtime_error("Failed to create server socket");
        }

        epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = this->servers[i];
        if (epoll_ctl(this->epoll_fd, EPOLL_CTL_ADD, this->servers[i], &ev) < 0)
        {
            perror("epoll_ctl");
            close(this->servers[i]);
            this->servers[i] = -1;
            cleanup();
            std::cerr << "[ERRO] Falha ao adicionar socket à lista epoll" << std::endl;
            throw std::runtime_error("Failed to add server socket to epoll");
        }
    }
}

Server::~Server()
{
    this->cleanup();
}

void Server::cleanup()
{
    if (cleaned_up)
    {
        std::cout << "[SERVER] cleanup() already called, skipping..." << std::endl;
        return;
    }
    cleaned_up = true;
    
    std::cout << "[SERVER] Shutting down..." << std::endl;

    for (std::map<int, Client *>::iterator it = clients.begin();
         it != clients.end(); ++it)
    {
        Client *client = it->second;
        if (client->isCgiActive())
            client->cleanupCgiIfActive(epoll_fd);
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, it->first, NULL);
        close(it->first);
        delete client;
    }
    clients.clear();

    for (std::map<int, Client *>::iterator it = cgi_fd_map.begin();
         it != cgi_fd_map.end(); ++it)
    {
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, it->first, NULL);
        close(it->first);
    }
    cgi_fd_map.clear();

    for (int i = 0; i < port_count; i++)
    {
        if (servers[i] >= 0)
        {
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, servers[i], NULL);
            close(servers[i]);
            std::cout << "[SERVER] Closed port " << ports[i] << std::endl;
        }
    }

    if (epoll_fd >= 0)
    {
        close(epoll_fd);
        std::cout << "[SERVER] Closed epoll fd" << std::endl;
    }

    delete[] ports;
    delete[] servers;
    delete[] interface;

    ports = NULL;
    servers = NULL;
    interface = NULL;
}

void Server::stop()
{
    std::cout << "[SERVER] Stop requested. Graceful shutdown in progress..." << std::endl;
    shutdown_requested = true;
}

void Server::start()
{
    std::cout << "Servidor iniciado. Aguardando conexões..." << std::endl;

    while (!shutdown_requested)
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

            std::map<int, Client *>::iterator it = clients.find(fd);
            if (it != clients.end())
            {
                Client *client = it->second;
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
                    handleClientSendReady(fd, client);
            }
            else
            {
                // fd pertence a um pipe CGI
                std::map<int, Client *>::iterator cit = cgi_fd_map.find(fd);
                if (cit != cgi_fd_map.end())
                {
                    Client *c = cit->second;
                    // ✅ Validar que o Client* ainda é válido (existe no clients map)
                    std::map<int, Client*>::iterator client_check = clients.find(c->getFd());
                    if (client_check != clients.end() && client_check->second == c)
                    {
                        // Client é válido, processar evento
                        handleCgiPipeEvent(c, events[i].events);
                    }
                    else
                    {
                        // Client foi deletado, remover do cgi_fd_map e ignorar evento
                        std::cout << "[EPOLLHUP] Ignorando evento para pipe fd=" << fd 
                                  << " (Client foi deletado)" << std::endl;
                        cgi_fd_map.erase(fd);
                    }
                }
            }
        }
    }
    
    std::cout << "[SERVER] Main loop exited. Cleaning up all resources..." << std::endl;
    this->cleanup();
}

void Server::handleClientSendReady(int fd, Client *client)
{
    if (client->getState() == Client::SENDING_RESPONSE && client->hasDataToSend())
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

void Server::handleCgiPipeEvent(Client *c, int events_mask)
{
    if (events_mask & (EPOLLIN | EPOLLERR | EPOLLHUP))
        c->handleCgiStdoutReadable(epoll_fd, cgi_fd_map);
    else if (events_mask & EPOLLOUT)
        c->handleCgiStdinWritable(epoll_fd);

    // Verificar se CGI terminou via EPOLLHUP
    if (events_mask & EPOLLHUP)
    {
        std::cout << "[EPOLLHUP] CGI pipe closed for client fd=" << c->getFd() << std::endl;

        Client::CgiState& cgi_state = c->getCgiState();
        int status;
        pid_t result = waitpid(cgi_state.pid, &status, WNOHANG);
        if (result == cgi_state.pid)
        {
            std::cout << "[CGI] Processo terminou (status=" << WEXITSTATUS(status) << ")" << std::endl;
            c->finishCgiAndGenerateResponse();
        }
        else if (result < 0)
        {
            std::cerr << "[CGI] waitpid error: " << strerror(errno) << std::endl;
            c->finishCgiAndGenerateResponse();
        }
    }
}
