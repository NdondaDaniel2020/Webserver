/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nmatondo <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/10/30 13:40:23 by nmatondo          #+#    #+#             */
/*   Updated: 2025/11/03 15:46:31 by nmatondo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"

#include <iostream>
#include <cstring>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <fcntl.h>



std::string create_error_message(const std::string& function_name)
{
    std::ostringstream oss;
    oss << function_name << " - " << strerror(errno);
    return oss.str();
}


void    webserv()
{
    epoll_event     ev;
    sockaddr_in     addr;
    epoll_event     events[64];
    Server          server(8080);
    int             epoll_fd;
    int             server_fd;


    addr = server.getAddr();
    server_fd = server.getServerFd();
    if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0)
        throw std::runtime_error(create_error_message("bind"));

    if (listen(server_fd, 10) < 0)
        throw std::runtime_error(create_error_message("listen"));

    std::cout << "Servidor EPOLL ouvindo na porta 8080..." << std::endl;
    
    epoll_fd = epoll_create(1);
    if (epoll_fd < 0)
        throw std::runtime_error(create_error_message("epoll_create"));

    ev.events = EPOLLIN;
    ev.data.fd = server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) < 0)
        throw std::runtime_error(create_error_message("epoll_ctl"));

    while (true)
    {
        int n = epoll_wait(epoll_fd, events, 64, -1);
        if (n < 0)
        {
            perror("epoll_wait");
            break;
        }
        for (int i = 0; i < n; i++)
        {
            if (events[i].data.fd == server_fd)
            {
                if (server.createNewClientConnection(epoll_fd))
                    continue;
            }
            else
                server.clientSentData();
        }
    }
}

int main(int ac, char **av)
{
    (void)ac;
    (void)av;
    try
    {
        webserv();
    }
    catch(const std::exception& e)
    {
        std::cerr << "Erro: " << e.what() << '\n';
    }
    return 0;
}