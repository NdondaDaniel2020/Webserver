/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ClientProcessing.cpp                               :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nmatondo <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/29 00:00:00 by copilot           #+#    #+#             */
/*   Updated: 2026/03/29 00:00:00 by copilot          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Client.hpp"
#include "StatusCodes.hpp"

static const LocationConfig *findMatchingLocation(const ServerConfig &config, const std::string &uri)
{
    const LocationConfig *best_match = NULL;
    size_t best_match_length = 0;

    for (size_t i = 0; i < config.locations.size(); i++)
    {
        const std::string &location_path = config.locations[i].path;
        if (uri.find(location_path) == 0 && location_path.size() > best_match_length)
        {
            best_match = &config.locations[i];
            best_match_length = location_path.size();
        }
    }

    return best_match;
}

void Client::processRequest(const ServerConfig &server_config, int epoll_fd)
{
    if (state == ERROR_413)
    {
        std::cout << "[CLIENT " << fd << "] Gerando resposta 413 (Content-Length excedeu limite)" << std::endl;

        std::string response_str;
        StatusCodes::http413PayloadTooLarge(response_str, server_config, this->request);

        send_buffer = response_str;
        send_offset = 0;
        state = SENDING_RESPONSE;
        keep_alive = false;
        return;
    }

    if (state != PROCESSING)
        return;

    if (!is_chunked && content_length > 0 && headers_end_pos > 0)
    {
        size_t body_size = recv_buffer.size() - headers_end_pos;
        if (body_size >= content_length)
        {
            std::string body = recv_buffer.substr(headers_end_pos, content_length);
            request.setBody(body);
            std::cout << "[CLIENT " << fd << "] Body extraido: " << body.size() << " bytes" << std::endl;
        }
    }

    const LocationConfig *location = findMatchingLocation(server_config, request.getUri());
    if (location && !location->cgi_handlers.empty())
    {
        bool method_allowed = false;
        if (!location->allowed_methods.empty())
        {
            for (size_t i = 0; i < location->allowed_methods.size(); ++i)
            {
                if (location->allowed_methods[i] == request.getMethod())
                {
                    method_allowed = true;
                    break;
                }
            }
        }

        if (!method_allowed)
        {
            std::cout << "[405] Metodo " << request.getMethod() << " nao permitido para: " << request.getUri() << std::endl;
            StatusCodes::http405MethodNotAllowed(this->send_buffer, request, request.getUri(), server_config);
            this->send_offset = 0;
            this->state = SENDING_RESPONSE;
            return;
        }

        std::cout << "[CLIENT " << fd << "] Iniciando CGI para " << request.getUri() << std::endl;
        state = CGI_RUNNING;
        startCgi(request, *location, server_config, epoll_fd);
        return;
    }

    if (response)
        delete response;

    response = new Response(request, server_config);
    send_buffer = response->getResponseHttp();
    send_offset = 0;

    state = SENDING_RESPONSE;

    std::cout << "[CLIENT " << fd << "] Resposta criada: "
              << send_buffer.size() << " bytes" << std::endl;
}
