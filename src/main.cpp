/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nmatondo <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/10/30 13:40:23 by nmatondo          #+#    #+#             */
/*   Updated: 2026/02/03 12:02:52 by nmatondo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <fcntl.h>
#include <cstring>
#include <iostream>
#include <unistd.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include "Server.hpp"

int main(int argc, char **argv)
{
    try
    {
        if (argc != 2)
        {
            throw std::runtime_error("Usage: " + std::string(argv[0]) + " <config_file>");
        }
        ConfigParser config_parser;
        if (!config_parser.loadFromFile(argv[1]))
            return 1;
        Server server(config_parser);
        server.start();
    }
    catch(const std::exception& e)
    {
        std::cerr << "Erro: " << e.what() << '\n';
    }
    return 0;
}
