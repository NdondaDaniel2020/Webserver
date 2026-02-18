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

Response::Response(const HttpRequest& request, const ServerConfig& config) : config(config)
{
    buildHttpResponse(request);
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

void Response::buildHttpResponse(const HttpRequest& request)
{
    // Construir caminho completo: root + uri
    std::string file_path = this->config.root + request.getUri();
    
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
