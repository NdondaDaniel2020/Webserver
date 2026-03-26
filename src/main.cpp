/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nmatondo <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/10/30 13:40:23 by nmatondo          #+#    #+#             */
/*   Updated: 2026/03/26 10:56:17 by nmatondo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include <csignal>

static Server* g_server_instance = NULL;
static volatile bool g_shutdown_requested = false;

void signal_handler(int sig)
{
    (void)sig;
    std::cout << "\n[SIGNAL] Ctrl+C received (SIGINT). Shutting down gracefully..." << std::endl;
    g_shutdown_requested = true;
    
    if (g_server_instance != NULL)
    {
        g_server_instance->requestStop();
    }
}

int main(int argc, char **argv)
{
    try
    {
        std::signal(SIGINT, signal_handler);
        
        if (argc != 2)
            throw std::runtime_error("Usage: " + std::string(argv[0]) + " <config_file>");
        ConfigParser config_parser;
        if (!config_parser.loadFromFile(argv[1]))
            throw std::runtime_error("Failed to load config file: " + std::string(argv[1]));
        Server server(config_parser);
        g_server_instance = &server;
        server.start();
        g_server_instance = NULL;
    }
    catch(const std::exception& e)
    {
        std::cerr << "Erro: " << e.what() << '\n';
        return (1);
    }
    return (0);
}
