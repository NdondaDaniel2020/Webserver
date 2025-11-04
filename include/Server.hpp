/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nmatondo <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/10/30 13:40:17 by nmatondo          #+#    #+#             */
/*   Updated: 2025/11/04 12:57:16 by nmatondo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SERVER_HPP
# define SERVER_HPP

# include <cerrno>
# include <cstdio>
# include <unistd.h>
# include <stdexcept>
# include <exception>
# include <sys/socket.h>
# include <netinet/in.h>
# include <sys/epoll.h>
# include <arpa/inet.h>
# include "FileUtils.hpp"



class Server
{
    private:
        int         server_fd;
        int         port;
        sockaddr_in addr;
        int         epoll_fd;
        epoll_event list_events[64];

    public:
        Server(int port);
        ~Server();

        void start();
        void stop();

    private:
        void setupSocket(); // Configurar Socket
        void setupEpoll(); // Configurar Epoll
        void handleEvents(); // Lidar com Eventos
        int  handleNewConnection(); // Lidar com Nova Conexão
        void handleClientData(int client_fd); // Lidar com Dados do Cliente
};

#endif
