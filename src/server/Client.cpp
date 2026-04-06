/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nmatondo <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/19 11:33:45 by nmatondo          #+#    #+#             */
/*   Updated: 2026/04/06 07:39:20 by nmatondo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Client.hpp"
#include "StatusCodes.hpp"


Client::Client(int fd, const ConfigParser *config, int server_index)
    : fd(fd),
      state(READING_HEADERS),
      last_activity(0), // ← corrigido
      recv_buffer(),
      send_buffer(),
      send_offset(0),
      request(),
      response(NULL),
      keep_alive(false),
      content_length(0),
      headers_end_pos(0),
      config(config),
      cgi(),
      is_cgi_active(false),
      is_chunked(false),
      check_valid_header(false),
      server_index(server_index),
      cgi_timeout(0)
{
    updateLastActivity();
}

Client::~Client()
{
    if (response)
        delete response;
    
    if (cgi.pid > 0)
    {
        std::cerr << "[WARNING] ~Client() cleaning up active CGI (pid=" << cgi.pid 
                  << "). Server should have called cleanupCgiIfActive()." << std::endl;
        kill(cgi.pid, SIGKILL);
        waitpid(cgi.pid, NULL, WNOHANG);
        cgi.pid = -1;
    }

    if (cgi.pipe_in[1] >= 0)
    {
        close(cgi.pipe_in[1]);
        cgi.pipe_in[1] = -1;
    }

    if (cgi.pipe_out[0] >= 0)
    {
        close(cgi.pipe_out[0]);
        cgi.pipe_out[0] = -1;
    }

    is_cgi_active = false;
    is_chunked = false;
}

void Client::cleanupCgiIfActive(int epoll_fd)
{
    if (!is_cgi_active)
        return;

    if (cgi.pid > 0)
    {
        kill(cgi.pid, SIGKILL);
        waitpid(cgi.pid, NULL, WNOHANG);
        cgi.pid = -1;
    }

    if (cgi.pipe_in[1] >= 0)
    {
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, cgi.pipe_in[1], NULL);
        close(cgi.pipe_in[1]);
        cgi.pipe_in[1] = -1;
    }

    if (cgi.pipe_out[0] >= 0)
    {
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, cgi.pipe_out[0], NULL);
        close(cgi.pipe_out[0]);
        cgi.pipe_out[0] = -1;
    }

    is_cgi_active = false;

    std::cout << "[CGI CLEANUP] Processo filho do cliente fd=" << fd
              << " terminado (SIGKILL enviado)" << std::endl;
}

int Client::getServerIndex()
{
    return server_index;
}

Client::CgiState &Client::getCgiState()
{
     return cgi;
}

// ========== Getters ==========

int Client::getFd() const
{
    return fd;
}

int Client::getCgiOutFd() const
{
    return cgi.pipe_out[0];
}

int Client::getCgiInFd() const
{
    return cgi.pipe_in[1];
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

bool Client::isCgiActive() const
{
    return state == CGI_RUNNING && cgi.pid > 0;
}

time_t Client::getLastActivity() const
{
    return last_activity;
}

time_t    Client::getCgiStartTime() const
{
    return cgi.start_time;
}

time_t              Client::getCgiTimeout() const
{
    return cgi_timeout;
}

const std::string& Client::getRecvBuffer() const
{
    return recv_buffer;
}

bool Client::hasDataToSend() const
{
    return !send_buffer.empty() && send_offset < send_buffer.size();
}

bool Client::sendData()
{
    if (state != SENDING_RESPONSE || !hasDataToSend())
        return false;

    ssize_t sent = write(fd, send_buffer.c_str() + send_offset, 
                        send_buffer.size() - send_offset);
    
    if (sent > 0) {
        send_offset += sent;
        updateLastActivity();
        
        if (send_offset >= send_buffer.size())
        {
            std::cout << "[CLIENT " << fd << "] Resposta enviada completamente" << std::endl;
            state = DONE;
            return true;
        }
        return false;
    }

    if (sent == 0)
    {
        std::cout << "[CLIENT " << fd << "] Conexão fechada pelo cliente durante envio" << std::endl;
        return false;
    }
    
    // sent < 0: erro ao escrever
    int err = errno;
    if (err == EAGAIN || err == EWOULDBLOCK)
    {
        // Socket bloqueado, retry posteriormente
        return false;
    }
    
    std::cerr << "[CLIENT " << fd << "] Erro ao enviar dados: " << strerror(err) << std::endl;
    return false;
}


void Client::sendTimeoutResponse()
{
    std::string response_str;
    StatusCodes::http504GatewayTimeout(response_str, request, config->getServerConfig(server_index));
    
    send_buffer = response_str;
    send_offset = 0;
    state = SENDING_RESPONSE;
    keep_alive = false;

    std::cout << "[CLIENT " << fd << "] Timeout response queued for sending (" 
              << send_buffer.size() << " bytes)" << std::endl;
}

// ========== Reset para keep-alive ==========

void Client::setState(State s) { state = s; }
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

    is_chunked = false;
    is_cgi_active = false;
    check_valid_header = false;
    cgi = CgiState();
    cgi_timeout = 0;
    
    state = READING_HEADERS;
    updateLastActivity();

    std::cout << "[CLIENT " << fd << "] Reset para keep-alive (is_cgi_active=false, is_chunked=false)" << std::endl;
}

// ========== Privados ==========

void Client::updateLastActivity()
{
    last_activity = time(NULL);
}