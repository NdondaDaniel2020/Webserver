/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nmatondo <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/10/30 13:40:20 by nmatondo          #+#    #+#             */
/*   Updated: 2025/11/04 12:58:34 by nmatondo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include <sstream>

Server::Server(const ServerConfig& config) : config(config)
{
    this->server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (this->server_fd < 0)
        throw std::runtime_error("socket");

    int opt = 1;
    setsockopt(this->server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    std::memset(&this->addr, 0, sizeof(this->addr));
    this->addr.sin_family = AF_INET;
    this->addr.sin_addr.s_addr = INADDR_ANY; 
    this->addr.sin_port = htons(this->config.port);
}

Server::~Server()
{
    close(this->server_fd);
}

void Server::start()
{
    this->setupSocket();
    this->setupEpoll();
    while (true)
    {
        this->handleEvents();
    }    
}

void Server::stop()
{
    close(this->epoll_fd);
    close(this->server_fd);
}

void Server::setupSocket() // Configurar Socket
{
    if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0)
        throw std::runtime_error(create_error_message("bind"));

    if (listen(server_fd, 10) < 0)
        throw std::runtime_error(create_error_message("listen"));
    
    std::cout << "Servidor '" << this->config.server_name << "' ouvindo na porta " << this->config.port << "..." << std::endl;
    std::cout << "Root: " << this->config.root << std::endl;

}

void Server::setupEpoll() // Configurar Epoll
{
    this->epoll_fd = epoll_create(1);
    if (this->epoll_fd < 0)
        throw std::runtime_error(create_error_message("epoll_create"));

    // Estrutura de evento
    epoll_event ev;
    ev.events = EPOLLIN;   // queremos saber quando tiver dados para ler
    ev.data.fd = this->server_fd;

    // Monitorar o socket do servidor
    if (epoll_ctl(this->epoll_fd, EPOLL_CTL_ADD, this->server_fd, &ev) < 0)
        throw std::runtime_error(create_error_message("epoll_ctl"));
}

void Server::handleEvents() // Lidar com Eventos
{
    int n = epoll_wait(this->epoll_fd, this->list_events, 64, -1);
    if (n < 0) { perror("epoll_wait"); return; }

    for (int i = 0; i < n; i++)
    {
        if (this->list_events[i].data.fd == this->server_fd)
        {
            if (this->handleNewConnection())
                continue;
        }
        else
        {
            this->handleClientData(this->list_events[i].data.fd);
        }
    }
}

int Server::handleNewConnection() // Lidar com Nova Conexão
{
    int client_fd = accept(this->server_fd, NULL, NULL);
    if (client_fd < 0) { perror("accept"); return 1; }

    std::cout << "[+] Cliente conectado: fd=" << client_fd << std::endl;

    epoll_event client_ev;
    client_ev.events = EPOLLIN;   // queremos ler dados dele
    client_ev.data.fd = client_fd;

    epoll_ctl(this->epoll_fd, EPOLL_CTL_ADD, client_fd, &client_ev);
    return 0;
}

void Server::handleClientData(int client_fd) // Lidar com Dados do Cliente
{
    char buf[4096];
    int r = read(client_fd, buf, sizeof(buf)-1);

    // Cliente desconectou
    if (r <= 0)
    {
        std::cout << "[-] Cliente desconectado: fd=" << client_fd << std::endl;
        close(client_fd);
        epoll_ctl(this->epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
        return;
    }

    buf[r] = '\0';
    HttpRequest req = parseHttpRequest(buf);
    
    // Construir e enviar resposta HTTP
    std::string response = buildHttpResponse(req);
    write(client_fd, response.c_str(), response.size());
    close(client_fd);
    epoll_ctl(this->epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
}

HttpRequest Server::parseHttpRequest(const std::string& raw_request)
{
    HttpRequest req;
    std::istringstream stream(raw_request);
    std::string line;
    
    // Parsear primeira linha: GET /index.html HTTP/1.1
    if (std::getline(stream, line))
    {
        // Remover \r no final
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        
        std::istringstream line_stream(line);
        line_stream >> req.method >> req.path >> req.version;
        
        // Se path é "/", usar o index file do config
        if (req.path == "/")
        {
            if (!this->config.index_files.empty())
                req.path = "/" + this->config.index_files[0];
            else
                req.path = "/index.html";
        }
        
        std::cout << "[REQUEST] " << req.method << " " << req.path << " " << req.version << std::endl;
    }
    
    return req;
}

std::string Server::buildHttpResponse(const HttpRequest& req)
{
    // Construir caminho completo: config.root + req.path
    std::string file_path = this->config.root + req.path;
    
    std::cout << "[FILE] Tentando ler: " << file_path << std::endl;
    
    // Tentar ler o arquivo
    std::string content = readFile(file_path);
    
    if (content.empty())
    {
        // Arquivo não encontrado - retornar 404
        std::cout << "[404] Arquivo não encontrado: " << file_path << std::endl;
        
        // Buscar página de erro 404 personalizada
        std::string error_page_404;
        std::map<std::string, std::string>::const_iterator it = this->config.error_pages.find("404");
        if (it != this->config.error_pages.end())
            error_page_404 = this->config.root + it->second;
        
        if (!error_page_404.empty())
            content = readFile(error_page_404);
        
        if (content.empty())
            content = "<html><body><h1>404 Not Found</h1></body></html>";
        
        std::ostringstream response;
        response << "HTTP/1.1 404 Not Found\r\n";
        response << "Content-Type: text/html; charset=UTF-8\r\n";
        response << "Content-Length: " << content.size() << "\r\n";
        response << "Connection: close\r\n";
        response << "\r\n";
        response << content;
        return response.str();
    }
    
    // Arquivo encontrado - retornar 200 OK
    std::cout << "[200] Arquivo encontrado: " << file_path << " (" << content.size() << " bytes)" << std::endl;
    
    std::ostringstream response;
    response << "HTTP/1.1 200 OK\r\n";
    response << "Content-Type: text/html; charset=UTF-8\r\n";
    response << "Content-Length: " << content.size() << "\r\n";
    response << "Connection: close\r\n";
    response << "\r\n";
    response << content;
    return response.str();
}
