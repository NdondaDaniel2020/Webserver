/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ClientRequest.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nmatondo <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/29 00:00:00 by copilot           #+#    #+#             */
/*   Updated: 2026/03/29 00:00:00 by copilot          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Client.hpp"

void Client::appendRecvData(const char *data, size_t len)
{
    if (recv_buffer.size() + len > MAX_RECV_BUFFER_SIZE)
    {
        std::cerr << "[CLIENT " << fd << "] recv_buffer excedeu limite ("
                  << recv_buffer.size() << " + " << len << " > "
                  << MAX_RECV_BUFFER_SIZE << ")" << std::endl;
        return;
    }
    recv_buffer.append(data, len);
    updateLastActivity();
}

bool Client::unchunkBody(std::string &out)
{
    const std::string &buffer = recv_buffer;
    size_t pos = headers_end_pos;

    while (pos < buffer.size())
    {
        size_t control_f = buffer.find("\r\n", pos);
        if (control_f == std::string::npos)
            return false;
        std::string value_hex = buffer.substr(pos, control_f - pos);
        size_t chunk_size = 0;
        std::stringstream iss(value_hex);
        iss >> std::hex >> chunk_size;
        pos = control_f + 2;
        if (chunk_size == 0)
            return true;
        if (pos + chunk_size + 2 > buffer.size())
            return false;
        out += buffer.substr(pos, chunk_size);
        pos += chunk_size + 2;
    }
    return false;
}

bool Client::isRequestComplete()
{
    if (state == READING_HEADERS)
    {
        if (findHeadersEnd())
        {
            parseHeaders();

            if (state == ERROR_413)
                return true;
            if (is_chunked)
                state = READING_BODY;
            else if ((request.getMethod() == "GET" ||
                      request.getMethod() == "POST" ||
                      request.getMethod() == "DELETE") &&
                     content_length == 0)
            {
                state = PROCESSING;
                return true;
            }
            else
                state = READING_BODY;
        }
    }

    if (state == READING_BODY)
    {
        if (is_chunked)
        {
            std::string unchunked_body;

            if (unchunkBody(unchunked_body))
            {
                size_t max_size = config->getServerConfig(getServerIndex()).client_max_body_size;
                if (max_size > 0 && unchunked_body.size() > max_size)
                {
                    request.setBody(unchunked_body);
                    state = ERROR_413;
                }
                else
                {
                    request.setBody(unchunked_body);
                    state = PROCESSING;
                }
                return true;
            }
        }
        else if (checkBodyComplete())
        {
            state = PROCESSING;
            return true;
        }
    }

    return false;
}

bool Client::findHeadersEnd()
{
    size_t pos = recv_buffer.find("\r\n\r\n");
    if (pos != std::string::npos)
    {
        headers_end_pos = pos + 4;
        return true;
    }
    return false;
}

void Client::parseHeaders()
{
    std::string headers_only = recv_buffer.substr(0, headers_end_pos);
    request = HttpRequest::parse(headers_only);

    if (request.hasHeader("Content-Length"))
    {
        std::istringstream iss(request.getHeader("Content-Length"));
        iss >> content_length;

        if (config)
        {
            size_t max_size = config->getServerConfig(getServerIndex()).client_max_body_size;
            if (max_size > 0 && content_length > max_size)
            {
                std::cout << "[413] Content-Length (" << content_length
                          << ") excede limite (" << max_size
                          << ") - Rejeitando ANTES de receber body" << std::endl;
                state = ERROR_413;
                return;
            }
        }
    }

    if (request.hasHeader("Connection"))
    {
        std::string conn = request.getHeader("Connection");
        keep_alive = (conn == "keep-alive" || conn == "Keep-Alive");
    }
    else
    {
        keep_alive = (request.getVersion() == "HTTP/1.1");
    }

    if (request.hasHeader("Transfer-Encoding"))
    {
        std::string transf_enco = request.getHeader("Transfer-Encoding");
        if (transf_enco == "chunked")
        {
            is_chunked = true;
            content_length = 0;
        }
    }
}

bool Client::checkBodyComplete()
{
    size_t body_received = recv_buffer.size() - headers_end_pos;
    return body_received >= content_length;
}
