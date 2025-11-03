/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nmatondo <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/10/30 13:40:20 by nmatondo          #+#    #+#             */
/*   Updated: 2025/11/03 15:52:20 by nmatondo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"

Server::Server(int port)
{
    this->server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (this->server_fd < 0)
        throw std::runtime_error("socket");

    this->opt = 1;
    setsockopt(this->server_fd, SOL_SOCKET, SO_REUSEADDR, &this->opt, sizeof(this->opt));

    std::memset(&this->addr, 0, sizeof(this->addr));
    this->addr.sin_family = AF_INET;
    this->addr.sin_addr.s_addr = INADDR_ANY; 
    this->addr.sin_port = htons(port);
}

Server::~Server()
{
    close(this->server_fd);
}

