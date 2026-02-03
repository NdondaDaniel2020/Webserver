/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nmatondo <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/10/30 13:40:17 by nmatondo          #+#    #+#             */
/*   Updated: 2026/02/03 12:06:48 by nmatondo         ###   ########.fr       */
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
# include "ConfigParser.hpp"
# include "Response.hpp"

class Server
{
    private:
        int* ports;
        int epoll_fd;
        int* servers;
        int port_count;
        epoll_event events[64];

    public:
        Server(const ConfigParser& config);
        ~Server();
        Server(const Server& other);
        Server& operator=(const Server& other);
        void start();
        void stop();

    private:
        int createServerSocket(int port);
        void newConnection(int fd);
        void handleClientData(int fd);
};

#endif
