/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nmatondo <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/10/30 13:40:17 by nmatondo          #+#    #+#             */
/*   Updated: 2026/03/24 15:09:30 by nmatondo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SERVER_HPP
# define SERVER_HPP

# include <cerrno>
# include <cstdio>
# include <unistd.h>
# include <stdexcept>
# include <exception>
# include <map>
# include <sys/socket.h>
# include <netinet/in.h>
# include <sys/epoll.h>
# include <arpa/inet.h>
# include "FileUtils.hpp"
# include "ConfigParser.hpp"
# include "Response.hpp"
# include "Client.hpp"

class Server
{
    private:
        int*                     ports;
        int                      epoll_fd;
        int*                     servers;
        int                      port_count;
        std::string*             interface;
        epoll_event              events[64];
        const ConfigParser&      config;
        std::map<int, Client*>   clients;      // fd cliente -> Client*
        std::map<int, Client*>   cgi_fd_map;   // fd pipe CGI -> Client*
        int                      TIMEOUT_SECONDS;
        int                      CGI_TIMEOUT_SECONDS;

    public:
        Server(const ConfigParser& config);
        ~Server();

        void cleanup();
        void start();
        void stop();

    private:
        int  createServerSocket(const std::string& interface, int port);
        void newConnection(int fd);
        void handleClientData(int fd);
        void closeClient(int fd);
        bool isServerSocket(int fd) const;
        void checkTimeout();
};

#endif