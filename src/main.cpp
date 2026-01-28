/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nmatondo <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/10/30 13:40:23 by nmatondo          #+#    #+#             */
/*   Updated: 2026/01/23 11:46:25 by nmatondo         ###   ########.fr       */
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

        int list_server = parser.getServerCount();
        ServerConfig list_server_config[list_server];
        std::cout << std::endl << "Number of servers configured: " << list_server << std::endl;
        for (int i = 0; i < list_server; i++)
        {
            list_server_config[i] = parser.getServerConfig(i);

            std::cout << "Starting server '" << list_server_config[i].server_name 
                      << "' on port " << list_server_config[i].port << "..." << std::endl;

            Server server(list_server_config[i]);
            server.start();
        }
        std::cout << std::endl;
    }
    catch(const std::exception& e)
    {
        std::cerr << "Erro: " << e.what() << '\n';
    }
    return 0;
}