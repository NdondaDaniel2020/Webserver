/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   StatusCodes.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nmatondo <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/28 11:30:00 by nmatondo          #+#    #+#             */
/*   Updated: 2026/03/18 13:17:17 by nmatondo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "StatusCodes.hpp"
#include <iostream>

std::string StatusCodes::findErrorPage(const std::map<std::string, std::string>& error_pages,
                                      const std::string& code)
{
    std::map<std::string, std::string>::const_iterator it = error_pages.find(code);
    if (it != error_pages.end())
        return it->second;

    if (code.size() == 3)
    {
        std::string generic_code = code.substr(0, 1) + "xx";
        it = error_pages.find(generic_code);
        if (it != error_pages.end())
            return it->second;
    }

    return "";
}

// ============================================================================
// RESPOSTAS DE SUCESSO (2xx)
// ============================================================================

void StatusCodes::http200Ok(std::string& response_str, const std::string& message)
{
    std::cout << "[200] OK - Requisição processada" << std::endl;
    
    std::ostringstream oss;
    oss << "HTTP/1.1 200 OK\r\n";
    oss << "Date: " << getCurrentHttpDate() << "\r\n";
    oss << "Server: webserv/1.0\r\n";
    oss << "Content-Type: application/json; charset=UTF-8\r\n";
    oss << "Content-Length: " << message.size() << "\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << message;
    response_str = oss.str();
}

void StatusCodes::http200FileFound(std::string& response_str, const HttpRequest& request, 
                                   const std::string& content, const std::string& file_path)
{
    std::cout << "[200] Arquivo encontrado: " << file_path << " (" << content.size() << " bytes)" << std::endl;

    std::string mime_type = getMimeType(file_path);
    std::string filename = getFileName(file_path);
    
    // Remover timestamp do nome do arquivo se existir
    size_t underscore_pos = filename.find('_');
    if (underscore_pos != std::string::npos && underscore_pos < 15)
    {
        bool is_timestamp = true;
        for (size_t i = 0; i < underscore_pos && i < filename.size(); ++i)
        {
            if (!isdigit(filename[i]))
            {
                is_timestamp = false;
                break;
            }
        }
        if (is_timestamp)
            filename = filename.substr(underscore_pos + 1);
    }

    std::ostringstream oss;
    oss << "HTTP/1.1 200 OK\r\n";
    oss << "Date: " << getCurrentHttpDate() << "\r\n";
    oss << "Server: webserv/1.0\r\n";
    // Adicionar charset para tipos de texto
    if (mime_type.find("text/") == 0)
        mime_type += "; charset=UTF-8";
    
    oss << "Content-Type: " << mime_type << "\r\n";
    oss << "Content-Length: " << content.size() << "\r\n";
    // oss << "Content-Disposition: attachment; filename=\"" << filename << "\"\r\n";
    oss << "Last-Modified: " << getFileModifiedDate(file_path) << "\r\n";
    if (request.getHeader("Connection") == "close")
        oss << "Connection: close\r\n";
    else if (request.getVersion() == "HTTP/1.0")
        oss << "Connection: close\r\n";
    else
        oss << "Connection: keep-alive\r\n";

    oss << "\r\n";
    oss << content;
    
    response_str = oss.str();
}

void StatusCodes::http201Created(std::string& response_str, const std::string& location, 
                                 const std::string& message)
{
    std::cout << "[201] Recurso criado: " << location << std::endl;
    
    std::ostringstream oss;
    oss << "HTTP/1.1 201 Created\r\n";
    oss << "Date: " << getCurrentHttpDate() << "\r\n";
    oss << "Server: webserv/1.0\r\n";
    oss << "Location: " << location << "\r\n";
    oss << "Content-Type: application/json; charset=UTF-8\r\n";
    oss << "Content-Length: " << message.size() << "\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << message;
    response_str = oss.str();
}

void StatusCodes::http204NoContent(std::string& response_str)
{
    std::cout << "[204] No Content - Recurso deletado" << std::endl;
    
    std::ostringstream oss;
    oss << "HTTP/1.1 204 No Content\r\n";
    oss << "Date: " << getCurrentHttpDate() << "\r\n";
    oss << "Server: webserv/1.0\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    response_str = oss.str();
}

// ============================================================================
// RESPOSTAS DE REDIRECIONAMENTO (3xx)
// ============================================================================

void StatusCodes::http301MovedPermanently(std::string& response_str, const std::string& location)
{
    std::cout << "[301] Moved Permanently: " << location << std::endl;
    
    std::string content = "<html><body><h1>301 Moved Permanently</h1><p>The resource has been moved to <a href='" + location + "'>" + location + "</a>.</p></body></html>";
    
    std::ostringstream oss;
    oss << "HTTP/1.1 301 Moved Permanently\r\n";
    oss << "Date: " << getCurrentHttpDate() << "\r\n";
    oss << "Server: webserv/1.0\r\n";
    oss << "Location: " << location << "\r\n";
    oss << "Content-Type: text/html; charset=UTF-8\r\n";
    oss << "Content-Length: " << content.size() << "\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << content;
    response_str = oss.str();
}

void StatusCodes::http302Found(std::string& response_str, const std::string& location)
{
    std::cout << "[302] Found: " << location << std::endl;
    
    std::string content = "<html><body><h1>302 Found</h1><p>The resource is temporarily located at <a href='" + location + "'>" + location + "</a>.</p></body></html>";
    
    std::ostringstream oss;
    oss << "HTTP/1.1 302 Found\r\n";
    oss << "Date: " << getCurrentHttpDate() << "\r\n";
    oss << "Server: webserv/1.0\r\n";
    oss << "Location: " << location << "\r\n";
    oss << "Content-Type: text/html; charset=UTF-8\r\n";
    oss << "Content-Length: " << content.size() << "\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << content;
    response_str = oss.str();
}

void StatusCodes::http304NotModified(std::string& response_str)
{
    std::cout << "[304] Not Modified" << std::endl;
    
    std::ostringstream oss;
    oss << "HTTP/1.1 304 Not Modified\r\n";
    oss << "Date: " << getCurrentHttpDate() << "\r\n";
    oss << "Server: webserv/1.0\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    response_str = oss.str();
}

// ============================================================================
// ERROS DO CLIENTE (4xx)
// ============================================================================

void StatusCodes::http400BadRequest(std::string& response_str, const std::string& message,
                                    const ServerConfig& config)
{
    std::cout << "[400] Bad Request: " << message << std::endl;

    // Buscar página de erro 400 personalizada
    std::string error_page_path = findErrorPage(config.error_pages, "400");
    std::string content;

    if (!error_page_path.empty())
    {
        std::string error_page_full = config.root + error_page_path;
        content = readFile(error_page_full);
    }

    if (content.empty())
    {
        std::ostringstream json_response;
        json_response << "{\"error\":\"Bad Request\",\"message\":\"" << message << "\"}";
        content = json_response.str();
    }

    std::ostringstream oss;
    oss << "HTTP/1.1 400 Bad Request\r\n";
    oss << "Date: " << getCurrentHttpDate() << "\r\n";
    oss << "Server: webserv/1.0\r\n";
    oss << "Content-Type: application/json; charset=UTF-8\r\n";
    oss << "Content-Length: " << content.size() << "\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << content;
    response_str = oss.str();
}

void StatusCodes::http401Unauthorized(std::string& response_str, const HttpRequest& request,
                                const std::string& message, const ServerConfig& config)
{
    std::cout << "[401] Unauthorized: " << message << std::endl;

    // Buscar página de erro 401 personalizada
    std::string error_page_path = findErrorPage(config.error_pages, "401");
    std::string content;

    if (!error_page_path.empty())
    {
        std::string error_page_full = config.root + error_page_path;
        content = readFile(error_page_full);
    }

    if (content.empty())
        content = "<html><body><h1>401 Unauthorized</h1><p>" + message + "</p></body></html>";

    std::ostringstream oss;
    oss << "HTTP/1.1 401 Unauthorized\r\n";
    oss << "Date: " << getCurrentHttpDate() << "\r\n";
    oss << "Server: webserv/1.0\r\n";
    oss << "WWW-Authenticate: Basic realm=\"Access to webserv\"\r\n";
    oss << "Content-Type: text/html; charset=UTF-8\r\n";
    oss << "Content-Length: " << content.size() << "\r\n";
    
    if (request.getHeader("Connection") == "close")
        oss << "Connection: close\r\n";
    else if (request.getVersion() == "HTTP/1.0")
        oss << "Connection: close\r\n";
    else
        oss << "Connection: keep-alive\r\n";
    
    oss << "\r\n";
    oss << content;
    response_str = oss.str();
}

void StatusCodes::http403Forbidden(std::string& response_str, const HttpRequest& request,
                                const std::string& file_path, const ServerConfig& config)
{
    std::cout << "[403] Acesso proibido: " << file_path << std::endl;

    // Buscar página de erro 403 personalizada
    std::string error_page_path = findErrorPage(config.error_pages, "403");
    std::string content;

    if (!error_page_path.empty())
    {
        std::string error_page_full = config.root + error_page_path;
        content = readFile(error_page_full);
    }

    if (content.empty())
        content = "<html><body><h1>403 Forbidden</h1><p>Access Denied</p></body></html>";

    std::ostringstream oss;
    oss << "HTTP/1.1 403 Forbidden\r\n";
    oss << "Date: " << getCurrentHttpDate() << "\r\n";
    oss << "Server: webserv/1.0\r\n";
    oss << "Content-Type: text/html; charset=UTF-8\r\n";
    oss << "Content-Length: " << content.size() << "\r\n";
    
    if (request.getHeader("Connection") == "close")
        oss << "Connection: close\r\n";
    else if (request.getVersion() == "HTTP/1.0")
        oss << "Connection: close\r\n";
    else
        oss << "Connection: keep-alive\r\n";
    
    oss << "\r\n";
    oss << content;
    response_str = oss.str();
}

void StatusCodes::http404NotFound(std::string& response_str, const HttpRequest& request,
                                  const std::string& file_path, const ServerConfig& config)
{
    std::cout << "[404] Arquivo não encontrado: " << file_path << std::endl;

    // Buscar página de erro 404 personalizada
    std::string error_page_path = findErrorPage(config.error_pages, "404");
    std::string content;
    if (!error_page_path.empty())
    {
        std::string error_page_full = config.root + error_page_path;
        content = readFile(error_page_full);   
    }
    
    if (content.empty())
        content = "<html><body><h1>404 Not Found</h1></body></html>";

    std::ostringstream oss;
    oss << "HTTP/1.1 404 Not Found\r\n";
    oss << "Date: " << getCurrentHttpDate() << "\r\n";
    oss << "Server: webserv/1.0\r\n";
    oss << "Content-Type: text/html; charset=UTF-8\r\n";
    oss << "Content-Length: " << content.size() << "\r\n";

    if (request.getHeader("Connection") == "close")
        oss << "Connection: close\r\n";
    else if (request.getVersion() == "HTTP/1.0")
        oss << "Connection: close\r\n";
    else
        oss << "Connection: keep-alive\r\n";

    oss << "\r\n";
    oss << content;
    response_str = oss.str();
}

void StatusCodes::http405MethodNotAllowed(std::string& response_str, const HttpRequest& request,
                                  const std::string& file_path, const ServerConfig& config)
{
    std::cout << "[405] Método não permitido para: " << file_path << std::endl;

    // Buscar página de erro 405 personalizada
    std::string error_page_path = findErrorPage(config.error_pages, "405");
    std::string content;

    if (!error_page_path.empty())
    {
        std::string error_page_full = config.root + error_page_path;
        content = readFile(error_page_full);
    }

    if (content.empty())
        content = "<html><body><h1>405 Method Not Allowed</h1></body></html>";

    std::ostringstream oss;
    oss << "HTTP/1.1 405 Method Not Allowed\r\n";
    oss << "Date: " << getCurrentHttpDate() << "\r\n";
    oss << "Server: webserv/1.0\r\n";
    oss << "Content-Type: text/html; charset=UTF-8\r\n";
    oss << "Content-Length: " << content.size() << "\r\n";

    if (request.getHeader("Connection") == "close")
        oss << "Connection: close\r\n";
    else if (request.getVersion() == "HTTP/1.0")
        oss << "Connection: close\r\n";
    else
        oss << "Connection: keep-alive\r\n";

    oss << "\r\n";
    oss << content;
    response_str = oss.str();
}

void StatusCodes::http408RequestTimeout(std::string& response_str, const ServerConfig& config)
{
    std::cout << "[408] Request Timeout" << std::endl;

    // Buscar página de erro 408 personalizada
    std::string error_page_path = findErrorPage(config.error_pages, "408");
    std::string content;

    if (!error_page_path.empty())
    {
        std::string error_page_full = config.root + error_page_path;
        content = readFile(error_page_full);
    }

    if (content.empty())
        content = "<html><body><h1>408 Request Timeout</h1><p>The server timed out waiting for the request.</p></body></html>";

    std::ostringstream oss;
    oss << "HTTP/1.1 408 Request Timeout\r\n";
    oss << "Date: " << getCurrentHttpDate() << "\r\n";
    oss << "Server: webserv/1.0\r\n";
    oss << "Content-Type: text/html; charset=UTF-8\r\n";
    oss << "Content-Length: " << content.size() << "\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << content;
    response_str = oss.str();
}

void StatusCodes::http409Conflict(std::string& response_str, const HttpRequest& request,
                                  const std::string& message, const ServerConfig& config)
{
    std::cout << "[409] Conflict: " << message << std::endl;

    // Buscar página de erro 409 personalizada
    std::string error_page_path = findErrorPage(config.error_pages, "409");
    std::string content;

    if (!error_page_path.empty())
    {
        std::string error_page_full = config.root + error_page_path;
        content = readFile(error_page_full);
    }

    if (content.empty())
        content = "<html><body><h1>409 Conflict</h1><p>" + message + "</p></body></html>";

    std::ostringstream oss;
    oss << "HTTP/1.1 409 Conflict\r\n";
    oss << "Date: " << getCurrentHttpDate() << "\r\n";
    oss << "Server: webserv/1.0\r\n";
    oss << "Content-Type: text/html; charset=UTF-8\r\n";
    oss << "Content-Length: " << content.size() << "\r\n";

    if (request.getHeader("Connection") == "close")
        oss << "Connection: close\r\n";
    else if (request.getVersion() == "HTTP/1.0")
        oss << "Connection: close\r\n";
    else
        oss << "Connection: keep-alive\r\n";

    oss << "\r\n";
    oss << content;
    response_str = oss.str();
}

void StatusCodes::http411LengthRequired(std::string& response_str, const ServerConfig& config)
{
    std::cout << "[411] Length Required" << std::endl;

    // Buscar página de erro 411 personalizada
    std::string error_page_path = findErrorPage(config.error_pages, "411");
    std::string content;

    if (!error_page_path.empty())
    {
        std::string error_page_full = config.root + error_page_path;
        content = readFile(error_page_full);
    }

    if (content.empty())
        content = "<html><body><h1>411 Length Required</h1><p>Content-Length header is required.</p></body></html>";

    std::ostringstream oss;
    oss << "HTTP/1.1 411 Length Required\r\n";
    oss << "Date: " << getCurrentHttpDate() << "\r\n";
    oss << "Server: webserv/1.0\r\n";
    oss << "Content-Type: text/html; charset=UTF-8\r\n";
    oss << "Content-Length: " << content.size() << "\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << content;
    response_str = oss.str();
}

void StatusCodes::http413PayloadTooLarge(std::string& response_str, const ServerConfig& config)
{
    std::cout << "[413] Payload Too Large" << std::endl;

    // Buscar página de erro 413 personalizada
    std::string error_page_path = findErrorPage(config.error_pages, "413");
    std::string content;

    if (!error_page_path.empty())
    {
        std::string error_page_full = config.root + error_page_path;
        content = readFile(error_page_full);
    }

    if (content.empty())
        content = "<html><body><h1>413 Payload Too Large</h1></body></html>";

    std::ostringstream oss;
    oss << "HTTP/1.1 413 Payload Too Large\r\n";
    oss << "Date: " << getCurrentHttpDate() << "\r\n";
    oss << "Server: webserv/1.0\r\n";
    oss << "Content-Type: text/html; charset=UTF-8\r\n";
    oss << "Content-Length: " << content.size() << "\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << content;
    response_str = oss.str();
}

void StatusCodes::http414UriTooLong(std::string& response_str, const ServerConfig& config)
{
    std::cout << "[414] URI Too Long" << std::endl;

    // Buscar página de erro 414 personalizada
    std::string error_page_path = findErrorPage(config.error_pages, "414");
    std::string content;

    if (!error_page_path.empty())
    {
        std::string error_page_full = config.root + error_page_path;
        content = readFile(error_page_full);
    }

    if (content.empty())
        content = "<html><body><h1>414 URI Too Long</h1><p>The requested URI exceeds the maximum length.</p></body></html>";

    std::ostringstream oss;
    oss << "HTTP/1.1 414 URI Too Long\r\n";
    oss << "Date: " << getCurrentHttpDate() << "\r\n";
    oss << "Server: webserv/1.0\r\n";
    oss << "Content-Type: text/html; charset=UTF-8\r\n";
    oss << "Content-Length: " << content.size() << "\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << content;
    response_str = oss.str();
}

void StatusCodes::http415UnsupportedMediaType(std::string& response_str, const HttpRequest& request, const ServerConfig& config)
{
    std::cout << "[415] Unsupported Media Type" << std::endl;

    std::string error_page_path = findErrorPage(config.error_pages, "415");
    std::string content;

    if (!error_page_path.empty())
    {
        std::string error_page_full = config.root + error_page_path;
        content = readFile(error_page_full);
    }

    if (content.empty())
        content = "<html><body><h1>415 Unsupported Media Type</h1></body></html>";

    std::ostringstream oss;
    oss << "HTTP/1.1 415 Unsupported Media Type\r\n";
    oss << "Date: " << getCurrentHttpDate() << "\r\n";
    oss << "Server: webserv/1.0\r\n";
    oss << "Content-Type: text/html; charset=UTF-8\r\n";
    oss << "Content-Length: " << content.size() << "\r\n";

    if (request.getHeader("Connection") == "close")
        oss << "Connection: close\r\n";
    else if (request.getVersion() == "HTTP/1.0")
        oss << "Connection: close\r\n";
    else
        oss << "Connection: keep-alive\r\n";

    oss << "\r\n";
    oss << content;
    response_str = oss.str();
}

void StatusCodes::http429TooManyRequests(std::string& response_str, const HttpRequest& request, const ServerConfig& config)
{
    std::cout << "[429] Too Many Requests" << std::endl;

    // Buscar página de erro 429 personalizada
    std::string error_page_path = findErrorPage(config.error_pages, "429");
    std::string content;

    if (!error_page_path.empty())
    {
        std::string error_page_full = config.root + error_page_path;
        content = readFile(error_page_full);
    }

    if (content.empty())
        content = "<html><body><h1>429 Too Many Requests</h1><p>Too many requests from this IP. Please try again later.</p></body></html>";

    std::ostringstream oss;
    oss << "HTTP/1.1 429 Too Many Requests\r\n";
    oss << "Date: " << getCurrentHttpDate() << "\r\n";
    oss << "Server: webserv/1.0\r\n";
    oss << "Retry-After: 60\r\n";
    oss << "Content-Type: text/html; charset=UTF-8\r\n";
    oss << "Content-Length: " << content.size() << "\r\n";

    if (request.getHeader("Connection") == "close")
        oss << "Connection: close\r\n";
    else if (request.getVersion() == "HTTP/1.0")
        oss << "Connection: close\r\n";
    else
        oss << "Connection: keep-alive\r\n";

    oss << "\r\n";
    oss << content;
    response_str = oss.str();
}

// ============================================================================
// ERROS DO SERVIDOR (5xx)
// ============================================================================

void StatusCodes::http500InternalServerError(std::string& response_str, const HttpRequest& request, const std::string& message,
                                             const ServerConfig& config)
{
    std::cout << "[500] Internal Server Error: " << message << std::endl;

    // Buscar página de erro 500 personalizada
    std::string error_page_path = findErrorPage(config.error_pages, "500");
    std::string content;

    if (!error_page_path.empty())
    {
        std::string error_page_full = config.root + error_page_path;
        content = readFile(error_page_full);
    }

    if (content.empty())
        content = "<html><body><h1>500 Internal Server Error</h1><p>" + message + "</p></body></html>";

    std::ostringstream oss;
    oss << "HTTP/1.1 500 Internal Server Error\r\n";
    oss << "Date: " << getCurrentHttpDate() << "\r\n";
    oss << "Server: webserv/1.0\r\n";
    oss << "Content-Type: text/html; charset=UTF-8\r\n";
    oss << "Content-Length: " << content.size() << "\r\n";
    if (request.getHeader("Connection") == "close")
        oss << "Connection: close\r\n";
    else if (request.getVersion() == "HTTP/1.0")
        oss << "Connection: close\r\n";
    else
        oss << "Connection: keep-alive\r\n";
    oss << "\r\n";
    oss << content;
    response_str = oss.str();
}

void StatusCodes::http501NotImplemented(std::string& response_str, const HttpRequest& request, const std::string& message,
                                        const ServerConfig& config)
{
    std::cout << "[501] Not Implemented: " << message << std::endl;

    // Buscar página de erro 501 personalizada
    std::string error_page_path = findErrorPage(config.error_pages, "501");
    std::string content;

    if (!error_page_path.empty())
    {
        std::string error_page_full = config.root + error_page_path;
        content = readFile(error_page_full);
    }

    if (content.empty())
        content = "<html><body><h1>501 Not Implemented</h1><p>" + message + "</p></body></html>";

    std::ostringstream oss;
    oss << "HTTP/1.1 501 Not Implemented\r\n";
    oss << "Date: " << getCurrentHttpDate() << "\r\n";
    oss << "Server: webserv/1.0\r\n";
    oss << "Content-Type: text/html; charset=UTF-8\r\n";
    oss << "Content-Length: " << content.size() << "\r\n";
    if (request.getHeader("Connection") == "close")
        oss << "Connection: close\r\n";
    else if (request.getVersion() == "HTTP/1.0")
        oss << "Connection: close\r\n";
    else
        oss << "Connection: keep-alive\r\n";
    oss << "\r\n";
    oss << content;
    response_str = oss.str();
}

void StatusCodes::http502BadGateway(std::string& response_str, const HttpRequest& request, const std::string& message,
                                    const ServerConfig& config)
{
    std::cout << "[502] Bad Gateway: " << message << std::endl;

    // Buscar página de erro 502 personalizada
    std::string error_page_path = findErrorPage(config.error_pages, "502");
    std::string content;

    if (!error_page_path.empty())
    {
        std::string error_page_full = config.root + error_page_path;
        content = readFile(error_page_full);
    }

    if (content.empty())
        content = "<html><body><h1>502 Bad Gateway</h1><p>" + message + "</p></body></html>";

    std::ostringstream oss;
    oss << "HTTP/1.1 502 Bad Gateway\r\n";
    oss << "Date: " << getCurrentHttpDate() << "\r\n";
    oss << "Server: webserv/1.0\r\n";
    oss << "Content-Type: text/html; charset=UTF-8\r\n";
    oss << "Content-Length: " << content.size() << "\r\n";
    if (request.getHeader("Connection") == "close")
        oss << "Connection: close\r\n";
    else if (request.getVersion() == "HTTP/1.0")
        oss << "Connection: close\r\n";
    else
        oss << "Connection: keep-alive\r\n";
    oss << "\r\n";
    oss << content;
    response_str = oss.str();
}

void StatusCodes::http503ServiceUnavailable(std::string& response_str, const HttpRequest& request, const ServerConfig& config)
{
    std::cout << "[503] Service Unavailable" << std::endl;

    // Buscar página de erro 503 personalizada
    std::string error_page_path = findErrorPage(config.error_pages, "503");
    std::string content;

    if (!error_page_path.empty())
    {
        std::string error_page_full = config.root + error_page_path;
        content = readFile(error_page_full);
    }

    if (content.empty())
        content = "<html><body><h1>503 Service Unavailable</h1><p>The server is temporarily unable to handle the request.</p></body></html>";

    std::ostringstream oss;
    oss << "HTTP/1.1 503 Service Unavailable\r\n";
    oss << "Date: " << getCurrentHttpDate() << "\r\n";
    oss << "Server: webserv/1.0\r\n";
    oss << "Retry-After: 120\r\n";
    oss << "Content-Type: text/html; charset=UTF-8\r\n";
    oss << "Content-Length: " << content.size() << "\r\n";
    if (request.getHeader("Connection") == "close")
        oss << "Connection: close\r\n";
    else if (request.getVersion() == "HTTP/1.0")
        oss << "Connection: close\r\n";
    else
        oss << "Connection: keep-alive\r\n";
    oss << "\r\n";
    oss << content;
    response_str = oss.str();
}

void StatusCodes::http504GatewayTimeout(std::string& response_str, const HttpRequest& request, const ServerConfig& config)
{
    std::cout << "[504] Gateway Timeout" << std::endl;

    // Buscar página de erro 504 personalizada
    std::string error_page_path = findErrorPage(config.error_pages, "504");
    std::string content;

    if (!error_page_path.empty())
    {
        std::string error_page_full = config.root + error_page_path;
        content = readFile(error_page_full);
    }

    if (content.empty())
        content = "<html><body><h1>504 Gateway Timeout</h1><p>The gateway did not receive a timely response from the upstream server.</p></body></html>";

    std::ostringstream oss;
    oss << "HTTP/1.1 504 Gateway Timeout\r\n";
    oss << "Date: " << getCurrentHttpDate() << "\r\n";
    oss << "Server: webserv/1.0\r\n";
    oss << "Content-Type: text/html; charset=UTF-8\r\n";
    oss << "Content-Length: " << content.size() << "\r\n";
    if (request.getHeader("Connection") == "close")
        oss << "Connection: close\r\n";
    else if (request.getVersion() == "HTTP/1.0")
        oss << "Connection: close\r\n";
    else
        oss << "Connection: keep-alive\r\n";
    oss << "\r\n";
    oss << content;
    response_str = oss.str();
}

void StatusCodes::http505VersionNotSupported(std::string& response_str, const ServerConfig& config)
{
    std::cout << "[505] HTTP Version Not Supported" << std::endl;

    // Buscar página de erro 505 personalizada
    std::string error_page_path = findErrorPage(config.error_pages, "505");
    std::string content;

    if (!error_page_path.empty())
    {
        std::string error_page_full = config.root + error_page_path;
        content = readFile(error_page_full);
    }

    if (content.empty())
        content = "<html><body><h1>505 HTTP Version Not Supported</h1><p>The HTTP version used in the request is not supported.</p></body></html>";

    std::ostringstream oss;
    oss << "HTTP/1.1 505 HTTP Version Not Supported\r\n";
    oss << "Date: " << getCurrentHttpDate() << "\r\n";
    oss << "Server: webserv/1.0\r\n";
    oss << "Content-Type: text/html; charset=UTF-8\r\n";
    oss << "Content-Length: " << content.size() << "\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << content;
    response_str = oss.str();
}
