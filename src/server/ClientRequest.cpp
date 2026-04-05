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
#include "ResponseHelpers.hpp"

void Client::appendRecvData(const char *data, size_t len)
{
    recv_buffer.append(data, len);
    updateLastActivity();
}

bool Client::IsHeaderRequestComplete()
{
    if (headers_end_pos == 0 && state == READING_HEADERS)
    {
        findHeadersEnd();
    }
    return headers_end_pos > 0;
}

bool Client::unchunkBody(std::string &out)
{
    const std::string &buffer = recv_buffer;
    size_t pos = headers_end_pos;
    size_t max_size = 0;

    if (config)
    {
        const ServerConfig &server_config = config->getServerConfig(getServerIndex());
        const LocationConfig *location = findMatchingLocation(server_config, request.getUri());
        
        if (location && location->client_max_body_size > 0)
            max_size = location->client_max_body_size;
        else if (server_config.client_max_body_size > 0)
            max_size = server_config.client_max_body_size;
    }

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

        if (max_size > 0 && out.size() + chunk_size > max_size)
        {
            state = ERROR_413;
            return true;
        }
        
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
            if (state == ERROR_413)
                return true;
            if (is_chunked)
                state = READING_BODY;
            else if ((request.getMethod() == "GET" ||
                      request.getMethod() == "POST" ||
                      request.getMethod() == "DELETE"))
            {
                if (request.getMethod() == "GET")
                {
                    state = PROCESSING;
                    return true;
                }
                state = READING_BODY;
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
                if (state != ERROR_413)
                {
                    size_t max_size = config->getServerConfig(getServerIndex()).client_max_body_size;
                    if (max_size > 0 && unchunked_body.size() > max_size)
                        state = ERROR_413;
                    else
                        state = PROCESSING;
                }
                request.setBody(unchunked_body);
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
            const ServerConfig &server_config = config->getServerConfig(getServerIndex());
            const LocationConfig *location = findMatchingLocation(server_config, request.getUri());
            
            size_t max_size = 0;
            if (location && location->client_max_body_size > 0)
                max_size = location->client_max_body_size;
            else if (server_config.client_max_body_size > 0)
                max_size = server_config.client_max_body_size;
            
            if (max_size > 0 && content_length > max_size)
            {
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

void Client::processHeaderRequest(const ServerConfig& server_config)
{
    if (check_valid_header)
        return;

    if (!IsHeaderRequestComplete())
        return;

    // SEMPRE processar headers PRIMEIRO (antes de validações que dependem deles)
    parseHeaders();
    
    if (state == ERROR_413)  // Content-Length ou Transfer-Encoding excedeu
        return;
    
    if (request.getMethod() != "GET" && request.getMethod() != "POST" && request.getMethod() != "DELETE")
    {
        StatusCodes::http405MethodNotAllowed(this->send_buffer, request, request.getUri(), server_config);
        this->send_offset = 0;
        this->state = SENDING_RESPONSE;
        return;
    }

    const LocationConfig *location = findMatchingLocation(server_config, request.getUri());
    if (!location)
    {
        StatusCodes::http404NotFound(this->send_buffer, request, request.getUri(), server_config);
        this->send_offset = 0;
        this->state = SENDING_RESPONSE;
        return;
    }

    if (!validateAllowedMethod(server_config, request))
    {
        StatusCodes::http405MethodNotAllowed(this->send_buffer, request, request.getUri(), server_config);
        this->send_offset = 0;
        this->state = SENDING_RESPONSE;
        return;
    }

    if (request.getMethod() == "POST" && !request.hasHeader("Content-Length") && !is_chunked)
    {
        StatusCodes::http411LengthRequired(this->send_buffer, server_config, request);
        this->send_offset = 0;
        this->state = SENDING_RESPONSE;
        return;
    }

    if (request.getMethod() == "POST" && !is_chunked && request.hasHeader("Content-Length"))
    {
        size_t max_size = 0;
        
        if (location && location->client_max_body_size > 0)
            max_size = location->client_max_body_size;
        else if (server_config.client_max_body_size > 0)
            max_size = server_config.client_max_body_size;

        if (max_size > 0 && content_length > max_size)
        {
            StatusCodes::http413PayloadTooLarge(this->send_buffer, server_config, request);
            this->send_offset = 0;
            this->state = SENDING_RESPONSE;
            return;
        }
    }

    if (request.getMethod() == "POST" && request.getHeader("Content-Type").empty())
    {
        StatusCodes::http400BadRequest(this->send_buffer, "Content-Type header is required", server_config, request);
        this->send_offset = 0;
        this->state = SENDING_RESPONSE;
        return;
    }

    if (request.getMethod() == "POST")
    {
        std::string content_type = request.getHeader("Content-Type");
        bool valid_type = 
            content_type.find("multipart/form-data") == 0 ||
            content_type.find("application/octet-stream") == 0 ||
            content_type.find("text/plain") == 0 ||
            content_type.find("test/file") == 0 ||
            content_type.find("application/x-www-form-urlencoded") == 0;
        
        if (!valid_type)
        {
            StatusCodes::http415UnsupportedMediaType(this->send_buffer, request, server_config);
            this->send_offset = 0;
            this->state = SENDING_RESPONSE;
            return;
        }
    }

    check_valid_header = true;
}
