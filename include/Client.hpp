/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nmatondo <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/19 11:30:38 by nmatondo          #+#    #+#             */
/*   Updated: 2026/04/06 07:57:14 by nmatondo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <iostream>
#include <string>
#include <vector>
#include <ctime>
#include <cstring>
#include <map> 

#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#include "HttpRequest.hpp"
#include "Response.hpp"
#include "ConfigParser.hpp"
#include "FileUtils.hpp"

# define BUFFERSIZE (256 * 1024)
# define EPOLL_MAX_EVENTS 1024

class Client
{
public:
    // Estados possíveis do cliente
    enum State
    {
        READING_HEADERS,    // Recebendo headers HTTP
        READING_BODY,       // Headers completos, recebendo body (POST)
        PROCESSING,         // Processando requisição (estático ou CGI)
        CGI_RUNNING,        // CGI em execução (non-blocking)
        SENDING_RESPONSE,   // Enviando resposta ao cliente
        DONE,               // Resposta enviada, pode fechar ou reutilizar
        ERROR_413           // Payload Too Large detectado cedo
    };

    struct CgiState
    {
        pid_t           pid;
        int             pipe_in[2];     // pai escreve body → filho stdin
        int             pipe_out[2];    // filho stdout → pai lê
        int             pipe_error[2];  // filho stderr → pai lê
        std::string     output;         // acumula saída do CGI
        std::string     error_output;   // acumula stderr do CGI
        size_t          body_written;   // quantos bytes do body já enviados
        bool            finished;
        time_t          start_time;

        CgiState() : pid(-1), body_written(0), finished(false), start_time(0)
        {
            pipe_in[0] = pipe_in[1] = -1;
            pipe_out[0] = pipe_out[1] = -1;
            pipe_error[0] = pipe_error[1] = -1;
        }
    };

private:
    int                 fd;
    State               state;
    time_t              last_activity;
    std::string         recv_buffer;
    std::string         send_buffer;
    size_t              send_offset;
    HttpRequest         request;
    Response*           response;
    bool                keep_alive;
    size_t              content_length;
    size_t              headers_end_pos;
    const ConfigParser* config;
    CgiState            cgi;
    bool                is_cgi_active;
    bool                is_chunked;
    bool                check_valid_header;
    int                 server_index;
    time_t              cgi_timeout;
    time_t              timeout;

public:
    Client(int fd, const ConfigParser* config, int server_index);
    ~Client();

    // Getters básicos
    int                 getFd() const;
    int                 getCgiErrFd() const;
    int                 getCgiOutFd() const;
    int                 getCgiInFd() const;
    State               getState() const;
    bool                isKeepAlive() const;
    bool                isDone() const;
    time_t              getLastActivity() const;
    time_t              getCgiStartTime() const;
    time_t              getCgiTimeout() const;
    time_t              getTimeout() const;
    bool                hasDataToSend() const;
    bool                isCgiActive() const;
    const std::string&  getRecvBuffer() const;
    void                cleanupCgiIfActive(int epoll_fd);
    int                 getServerIndex();

    CgiState&           getCgiState();

    void                sendTimeoutResponse();
    void                appendRecvData(const char* data, size_t len);
    bool                isRequestComplete();
    bool                IsHeaderRequestComplete();
    bool                isHeaderValidated() const { return check_valid_header; }

    void                processHeaderRequest(const ServerConfig& server_config);
    void                processRequest(const ServerConfig& server_config, int epoll_fd);
    int                 sendData();
    void                reset();
    void                setState(State s);

    // CGI non-blocking
    void                startCgi(const HttpRequest& req, const LocationConfig& loc,
                                 const ServerConfig& server_config, int epoll_fd);
    void                handleCgiStdoutReadable(int epoll_fd, std::map<int, Client*>& cgi_fd_map);
    void                handleCgiStderrReadable(int epoll_fd, std::map<int, Client*>& cgi_fd_map);
    void                handleCgiStdinWritable(int epoll_fd);
    void                finishCgiAndGenerateResponse();
    bool                unchunkBody(std::string& out);

private:
    bool                findHeadersEnd();
    bool                checkBodyComplete();
    void                parseHeaders();
    void                updateLastActivity();
};

#endif