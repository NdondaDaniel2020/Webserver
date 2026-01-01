/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nmatondo <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/10/30 13:40:23 by nmatondo          #+#    #+#             */
/*   Updated: 2025/11/04 12:46:26 by nmatondo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"

#include <iostream>
#include <cstring>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <fcntl.h>

int main(int argc, char **argv)
{
    try
    {
        if (argc != 2)
        {
            throw std::runtime_error("Usage: " + std::string(argv[0]) + " <config_file>");
        }
        ConfigParser parser;
        if (!parser.loadFromFile(argv[1]))
            return 1;
        ServerConfig server_config = parser.getServerConfig(0);
        Server server(server_config);
        server.start();
    }
    catch(const std::exception& e)
    {
        std::cerr << "Erro: " << e.what() << '\n';
    }
    return 0;
}