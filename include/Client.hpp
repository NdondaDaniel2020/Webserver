/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nmatondo <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/19 11:30:38 by nmatondo          #+#    #+#             */
/*   Updated: 2026/02/28 12:05:45 by nmatondo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <iostream>
#include <string>
#include <vector>
#include <ctime>
#include <cstring>
#include <map>           // se usares map em algum lugar, senão remove

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

    // Estrutura para gerir o estado do CGI (non-blocking)
    struct CgiState
    {
        pid_t           pid;
        int             pipe_in[2];     // pai escreve body → filho stdin
        int             pipe_out[2];    // filho stdout → pai lê
        std::string     output;         // acumula saída do CGI
        size_t          body_written;   // quantos bytes do body já enviados
        bool            finished;
        time_t          start_time;

        CgiState() : pid(-1), body_written(0), finished(false), start_time(0)
        {
            pipe_in[0] = pipe_in[1] = -1;
            pipe_out[0] = pipe_out[1] = -1;
        }
    };

private:
    int                 fd;
    State               state;
    time_t              last_activity;          // ← posição 3
    std::string         recv_buffer;
    std::string         send_buffer;
    size_t              send_offset;
    HttpRequest         request;
    Response*           response;
    bool                keep_alive;
    size_t              content_length;         // ← posição 10
    size_t              headers_end_pos;
    const ConfigParser* config;
    CgiState            cgi;
    bool                is_cgi_active;

public:
    // Construtor / Destrutor / Copia
    Client(int fd, const ConfigParser* config);
    ~Client();
    Client(const Client& other);
    Client& operator=(const Client& other);

    // Getters básicos
    int                 getFd() const;
    int                 getCgiOutFd() const;
    int                 getCgiInFd() const;
    State               getState() const;
    bool                isKeepAlive() const;
    bool                isDone() const;
    time_t              getLastActivity() const;
    bool                hasDataToSend() const;
    bool                isCgiActive() const { return is_cgi_active; }
    void                cleanupCgiIfActive(int epoll_fd);

    // FDs do CGI (para epoll no Server)

    // Recepção de dados
    void                appendRecvData(const char* data, size_t len);
    bool                isRequestComplete();

    // Processamento da requisição
    void                processRequest(const ServerConfig& server_config, int epoll_fd);

    // Envio de dados
    bool                sendData();

    // Reset para keep-alive
    void                reset();
    void                setState(State s);

    // CGI non-blocking
    void                startCgi(const HttpRequest& req, const LocationConfig& loc, int epoll_fd);
    void                handleCgiStdoutReadable(int epoll_fd);
    void                handleCgiStdinWritable(int epoll_fd);
    void                finishCgiAndGenerateResponse(int epoll_fd);

private:
    // Helpers internos
    bool                findHeadersEnd();
    bool                checkBodyComplete();
    void                parseHeaders();
    void                updateLastActivity();
};

#endif // CLIENT_HPP