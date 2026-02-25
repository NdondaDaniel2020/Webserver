/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Response.cpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nmatondo <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/03 10:05:33 by nmatondo          #+#    #+#             */
/*   Updated: 2026/02/25 15:32:35 by nmatondo         ###   ########.fr       */
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
    // 1. Sanitizar URI para prevenir path traversal
    std::string uri = sanitizePath(request.getUri());
    std::string file_path = this->config.root + uri;

    // 2. Validar segurança - verificar se o caminho está dentro do root
    if (!isPathSafe(file_path, this->config.root))
    {
        std::cout << "[403] Path traversal bloqueado: " << request.getUri() << std::endl;
        return httpForbidden403(file_path);
    }
    if (request.getMethod() == "GET")
        methodGet(request, file_path);
    else if (request.getMethod() == "POST")
        methodPost(request, file_path);
    else if (request.getMethod() == "DELETE")
        methodDelete(request, file_path);
    else
        methodNotAllowed405(file_path);
}

void Response::methodGet(const HttpRequest& request, const std::string& file_path)
{
    std::string _file_path = file_path;
    // 3. Verificar se é diretório
    if (isDirectory(_file_path))
    {
        // Buscar arquivo index configurado (index.html, etc)
        std::string index_path = findIndexFile(_file_path, this->config.index_files);
        
        if (!index_path.empty())
        {
            _file_path = index_path;
        }
        else
        {
            // TODO: Implementar autoindex se config permitir
            std::cout << "[403] Diretório sem arquivo index: " << _file_path << std::endl;
            return httpForbidden403(_file_path);
        }
    }
    
    // 4. Verificar se arquivo existe
    if (!fileExists(_file_path))
    {
        return httpFileNotFound404(request, "", _file_path);
    }
    
    // 5. Verificar permissões de leitura
    if (!isReadable(_file_path))
    {
        std::cout << "[403] Sem permissão de leitura: " << _file_path << std::endl;
        return httpForbidden403(_file_path);
    }
    
    // 6. Ler arquivo e retornar
    std::string content = readFile(_file_path);
    httpFileFound200(request, content, _file_path);
}

void Response::methodPost(const HttpRequest& request, const std::string& file_path)
{
    (void)request;
    (void)file_path;
}

void Response::methodDelete(const HttpRequest& request, const std::string& file_path)
{
    (void)request;
    (void)file_path;
}



void Response::httpFileNotFound404(const HttpRequest& request, const std::string& content, const std::string& file_path)
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
    oss << "Date: " << getCurrentHttpDate() << "\r\n";
    oss << "Server: webserv/1.0\r\n";
    oss << "Content-Type: text/html; charset=UTF-8\r\n";
    oss << "Content-Length: " << final_content.size() << "\r\n";
    
    if (request.getHeader("Connection") != "" && request.getHeader("Connection") == "keep-alive")
        oss << "Connection: keep-alive\r\n";
    else
        oss << "Connection: close\r\n";

    oss << "\r\n";
    oss << final_content;
    this->response_str = oss.str();
}

void Response::httpFileFound200(const HttpRequest& request, const std::string& content, const std::string& file_path)
{
    // Arquivo encontrado - retornar 200 OK
    std::cout << "[200] Arquivo encontrado: " << file_path << " (" << content.size() << " bytes)" << std::endl;

    // Detectar MIME type correto baseado na extensão
    std::string mime_type = getMimeType(file_path);

    std::ostringstream oss;
    oss << "HTTP/1.1 200 OK\r\n";
    oss << "Date: " << getCurrentHttpDate() << "\r\n";
    oss << "Server: webserv/1.0\r\n";
    oss << "Content-Type: " << mime_type << "\r\n";
    oss << "Content-Length: " << content.size() << "\r\n";
    oss << "Last-Modified: " << getFileModifiedDate(file_path) << "\r\n";

    if (request.getHeader("Connection") != "" && request.getHeader("Connection") == "keep-alive")
        oss << "Connection: keep-alive\r\n";
    else
        oss << "Connection: close\r\n";

    oss << "\r\n";
    oss << content;
    this->response_str = oss.str();
}

void Response::methodNotAllowed405(const std::string& file_path)
{
    // Método não permitido - retornar 405 Method Not Allowed
    std::cout << "[405] Método não permitido para: " << file_path << std::endl;

    std::ostringstream oss;
    oss << "HTTP/1.1 405 Method Not Allowed\r\n";
    oss << "Date: " << getCurrentHttpDate() << "\r\n";
    oss << "Server: webserv/1.0\r\n";
    oss << "Content-Type: text/html; charset=UTF-8\r\n";
    oss << "Content-Length: 0\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    this->response_str = oss.str();
}

void Response::httpForbidden403(const std::string& file_path)
{
    // Acesso proibido - retornar 403 Forbidden
    std::cout << "[403] Acesso proibido: " << file_path << std::endl;
    
    std::string content = "<html><body><h1>403 Forbidden</h1><p>Access Denied</p></body></html>";
    
    std::ostringstream oss;
    oss << "HTTP/1.1 403 Forbidden\r\n";
    oss << "Date: " << getCurrentHttpDate() << "\r\n";
    oss << "Server: webserv/1.0\r\n";
    oss << "Content-Type: text/html; charset=UTF-8\r\n";
    oss << "Content-Length: " << content.size() << "\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << content;
    this->response_str = oss.str();
}



