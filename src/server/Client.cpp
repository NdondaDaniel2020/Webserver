/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nmatondo <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/19 11:33:45 by nmatondo          #+#    #+#             */
/*   Updated: 2026/02/26 12:49:45 by nmatondo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Client.hpp"
#include <iostream>
#include <cstring>

// ========== Constructor / Destructor ==========

Client::Client(int fd, const ConfigParser* config)
    : fd(fd), state(READING_HEADERS), send_offset(0), response(NULL),
      keep_alive(false), content_length(0), headers_end_pos(0), config(config)
{
    updateLastActivity();
}

Client::~Client()
{
    if (response)
        delete response;
}

Client::Client(const Client& other)
    : fd(other.fd), state(other.state), recv_buffer(other.recv_buffer),
      send_buffer(other.send_buffer), send_offset(other.send_offset),
      request(other.request), response(NULL), keep_alive(other.keep_alive),
      content_length(other.content_length), headers_end_pos(other.headers_end_pos),
      last_activity(other.last_activity), config(other.config)
{
    if (other.response)
        response = new Response(*other.response);
}

Client& Client::operator=(const Client& other)
{
    if (this != &other)
    {
        fd = other.fd;
        state = other.state;
        recv_buffer = other.recv_buffer;
        send_buffer = other.send_buffer;
        send_offset = other.send_offset;
        request = other.request;
        
        if (response)
            delete response;
        response = other.response ? new Response(*other.response) : NULL;
        
        keep_alive = other.keep_alive;
        content_length = other.content_length;
        headers_end_pos = other.headers_end_pos;
        last_activity = other.last_activity;
        config = other.config;
    }
    return *this;
}

// ========== Getters ==========

int Client::getFd() const
{
    return fd;
}

Client::State Client::getState() const
{
    return state;
}

bool Client::isKeepAlive() const
{
    return keep_alive;
}

bool Client::isDone() const
{
    return state == DONE;
}

time_t Client::getLastActivity() const
{
    return last_activity;
}

bool Client::hasDataToSend() const
{
    return !send_buffer.empty() && send_offset < send_buffer.size();
}

// ========== Recepção de dados ==========

void Client::appendRecvData(const char* data, size_t len)
{
    recv_buffer.append(data, len);
    updateLastActivity();
}

bool Client::isRequestComplete()
{
    if (state == READING_HEADERS)
    {
        if (findHeadersEnd())
        {
            parseHeaders();
            
            // Se não tem body (GET, POST, DELETE) e Content-Length: 0
            if ((request.getMethod() == "GET" || 
                request.getMethod() == "POST" ||
                request.getMethod() == "DELETE") &&
                content_length == 0)
            {
                state = PROCESSING;
                return true;
            }
            
            // Se tem body, muda para READING_BODY
            state = READING_BODY;
        }
    }
    
    if (state == READING_BODY)
    {
        if (checkBodyComplete())
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
        headers_end_pos = pos + 4;  // +4 para pular o \r\n\r\n
        return true;
    }
    return false;
}

void Client::parseHeaders()
{
    // Parse apenas os headers (até headers_end_pos)
    std::string headers_only = recv_buffer.substr(0, headers_end_pos);
    request = HttpRequest::parse(headers_only);
    
    // Verificar Content-Length
    if (request.hasHeader("Content-Length"))
    {
        std::istringstream iss(request.getHeader("Content-Length"));
        iss >> content_length;
    }
    
    // Verificar keep-alive
    if (request.hasHeader("Connection"))
    {
        std::string conn = request.getHeader("Connection");
        keep_alive = (conn == "keep-alive" || conn == "Keep-Alive");
    }
    else
    {
        // HTTP/1.1 default é keep-alive
        keep_alive = (request.getVersion() == "HTTP/1.1");
    }
}

bool Client::checkBodyComplete()
{
    // Body começa após os headers
    size_t body_received = recv_buffer.size() - headers_end_pos;
    return body_received >= content_length;
}

// ========== Processamento ==========

void Client::processRequest(const ServerConfig& server_config)
{
    if (state != PROCESSING)
        return;
    
    // Se tem body, extrair do recv_buffer e adicionar ao request
    if (content_length > 0 && headers_end_pos > 0)
    {
        size_t body_size = recv_buffer.size() - headers_end_pos;
        if (body_size >= content_length)
        {
            std::string body = recv_buffer.substr(headers_end_pos, content_length);
            request.setBody(body);
            std::cout << "[CLIENT " << fd << "] Body extraído: " << body.size() << " bytes" << std::endl;
        }
    }
    
    // Criar resposta
    if (response)
        delete response;
    
    response = new Response(request, server_config);
    send_buffer = response->getResponseHttp();
    send_offset = 0;
    
    state = SENDING_RESPONSE;
    
    std::cout << "[CLIENT " << fd << "] Resposta criada: " 
              << send_buffer.size() << " bytes" << std::endl;
}

// ========== Envio de resposta ==========

bool Client::sendData()
{
    if (state != SENDING_RESPONSE || !hasDataToSend())
        return false;
    
    // Enviar chunk do buffer
    size_t remaining = send_buffer.size() - send_offset;
    size_t to_send = remaining;  // Pode limitar aqui (ex: 8192 bytes por vez)
    
    ssize_t sent = write(fd, send_buffer.c_str() + send_offset, to_send);
    
    if (sent < 0)
    {
        std::cerr << "[CLIENT " << fd << "] Erro ao enviar dados" << std::endl;
        return false;
    }
    
    send_offset += sent;
    updateLastActivity();
    
    // Verificar se terminou de enviar
    if (send_offset >= send_buffer.size())
    {
        std::cout << "[CLIENT " << fd << "] Resposta enviada completamente" << std::endl;
        state = DONE;
        return true;
    }
    
    return false;
}

// ========== Reset para keep-alive ==========

void Client::reset()
{
    recv_buffer.clear();
    send_buffer.clear();
    send_offset = 0;
    
    if (response)
    {
        delete response;
        response = NULL;
    }
    
    request = HttpRequest();
    content_length = 0;
    headers_end_pos = 0;
    state = READING_HEADERS;
    
    updateLastActivity();
    
    std::cout << "[CLIENT " << fd << "] Reset para keep-alive" << std::endl;
}

// ========== Privados ==========

void Client::updateLastActivity()
{
    last_activity = time(NULL);
}
