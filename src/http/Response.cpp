/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Response.cpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nmatondo <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/03 10:05:33 by nmatondo          #+#    #+#             */
/*   Updated: 2026/02/03 11:47:11 by nmatondo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Response.hpp"

Response::Response(const std::string &request, const ServerConfig& config) : config(config)
{
    buildHttpResponse(parseHttpRequest(request));
}

Response::~Response()
{
}

Response::Response(const Response &other)
{
    *this = other;
}

Response &Response::operator=(const Response &other)
{
    if (this != &other)
    {
        this->response_str = other.response_str;
        this->config = other.config;
    }
    return *this;
}

std::string Response::getResponseHttp()
{
    return this->response_str;
}

HttpRequest Response::parseHttpRequest(const std::string &raw_request)
{
    HttpRequest req;
    std::istringstream stream(raw_request);
    std::string line;
    
    std::cout << "[RAW REQUEST]\n" << raw_request << std::endl;
    // Parsear primeira linha: GET /index.html HTTP/1.1
    if (std::getline(stream, line))
    {
        // Remover \r no final
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        
        std::istringstream line_stream(line);
        line_stream >> req.method >> req.path >> req.version;
        
        // Se path é "/", usar o index file
        if (req.path == "/")
        {
            req.path = "/index.html";
        }
        
        std::cout << "[REQUEST] " << req.method << " " << req.path << " " << req.version << std::endl;
    }
    
    return req;
}

void Response::buildHttpResponse(const HttpRequest& request)
{
    // Construir caminho completo: root + req.path
    std::string file_path = this->config.root + request.path;
    
    std::cout << "[FILE] Tentando ler: " << file_path << std::endl;
    
    // Tentar ler o arquivo
    std::string content = readFile(file_path);
    
    if (content.empty())
    {
        httpFileNotFound(content, file_path);
        return;
    }
    httpFileFound(content, file_path);
}

void Response::httpFileNotFound(const std::string& content, const std::string& file_path)
{
    // Arquivo não encontrado - retornar 404
    std::cout << "[404] Arquivo não encontrado: " << file_path << std::endl;
    
    // Buscar página de erro 404 personalizada
    std::string error_page_404;
    std::map<std::string, std::string>::const_iterator it = this->config.error_pages.find("404");
    if (it != this->config.error_pages.end())
        error_page_404 = this->config.root + it->second;
    
    std::string final_content = content;
    if (!error_page_404.empty())
        final_content = readFile(error_page_404);
    
    if (final_content.empty())
        final_content = "<html><body><h1>404 Not Found</h1></body></html>";
    
    std::ostringstream oss;
    oss << "HTTP/1.1 404 Not Found\r\n";
    oss << "Content-Type: text/html; charset=UTF-8\r\n";
    oss << "Content-Length: " << final_content.size() << "\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << final_content;
    this->response_str = oss.str();
}

void Response::httpFileFound(const std::string& content, const std::string& file_path)
{
    // Arquivo encontrado - retornar 200 OK
    std::cout << "[200] Arquivo encontrado: " << file_path << " (" << content.size() << " bytes)" << std::endl;

    std::ostringstream oss;
    oss << "HTTP/1.1 200 OK\r\n";
    oss << "Content-Type: text/html; charset=UTF-8\r\n";
    oss << "Content-Length: " << content.size() << "\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << content;
    this->response_str = oss.str();
}
