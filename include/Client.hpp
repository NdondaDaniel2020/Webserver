/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nmatondo <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/19 11:30:38 by nmatondo          #+#    #+#             */
/*   Updated: 2026/02/19 12:33:49 by nmatondo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CLIENT_HPP
# define CLIENT_HPP

# include <string>
# include <ctime>
# include <unistd.h>
# include "HttpRequest.hpp"
# include "Response.hpp"
# include "ConfigParser.hpp"

class Client
{
    public:
        enum State {
            READING_HEADERS,    // Ainda recebendo headers HTTP
            READING_BODY,       // Headers completos, recebendo body (POST)
            PROCESSING,         // Processando requisição
            SENDING_RESPONSE,   // Enviando resposta ao cliente
            DONE,               // Resposta enviada, pode fechar ou reutilizar
            ERROR_413           // Payload Too Large - Content-Length excedeu limite
        };

    private:
        int fd;
        State state;
        std::string recv_buffer;        // Acumula dados recebidos
        std::string send_buffer;        // Dados a enviar
        size_t send_offset;             // Posição no buffer de envio
        HttpRequest request;
        Response* response;
        bool keep_alive;
        size_t content_length;          // Content-Length do body
        size_t headers_end_pos;         // Posição onde terminam os headers
        time_t last_activity;           // Para timeout
        const ConfigParser* config;     // Configuração do servidor

    public:
        Client(int fd, const ConfigParser* config);
        ~Client();
        Client(const Client& other);
        Client& operator=(const Client& other);

        // Getters
        int getFd() const;
        State getState() const;
        bool isKeepAlive() const;
        bool isDone() const;
        time_t getLastActivity() const;
        bool hasDataToSend() const;

        // Recepção de dados
        void appendRecvData(const char* data, size_t len);
        bool isRequestComplete();

        // Processamento
        void processRequest(const ServerConfig& server_config);

        // Envio de resposta
        bool sendData();

        // Reset para keep-alive
        void reset();

    private:
        bool findHeadersEnd();
        bool checkBodyComplete();
        void parseHeaders();
        void updateLastActivity();
};

#endif