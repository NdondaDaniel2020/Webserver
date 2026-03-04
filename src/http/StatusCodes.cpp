/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   StatusCodes.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nmatondo <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/28 11:30:00 by nmatondo          #+#    #+#             */
/*   Updated: 2026/03/04 10:18:27 by nmatondo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "StatusCodes.hpp"
#include <iostream>

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

void StatusCodes::http400BadRequest(std::string& response_str, const std::string& message)
{
    std::cout << "[400] Bad Request: " << message << std::endl;
    
    std::ostringstream json_response;
    json_response << "{\"error\":\"Bad Request\",\"message\":\"" << message << "\"}";
    std::string content = json_response.str();
    
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

void StatusCodes::http401Unauthorized(std::string& response_str, const std::string& message)
{
    std::cout << "[401] Unauthorized: " << message << std::endl;
    
    std::string content = "<html><body><h1>401 Unauthorized</h1><p>" + message + "</p></body></html>";
    
    std::ostringstream oss;
    oss << "HTTP/1.1 401 Unauthorized\r\n";
    oss << "Date: " << getCurrentHttpDate() << "\r\n";
    oss << "Server: webserv/1.0\r\n";
    oss << "WWW-Authenticate: Basic realm=\"Access to webserv\"\r\n";
    oss << "Content-Type: text/html; charset=UTF-8\r\n";
    oss << "Content-Length: " << content.size() << "\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << content;
    response_str = oss.str();
}

void StatusCodes::http403Forbidden(std::string& response_str, const std::string& file_path)
{
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
    response_str = oss.str();
}

void StatusCodes::http404NotFound(std::string& response_str, const HttpRequest& request, 
                                  const std::string& content, const std::string& file_path,
                                  const ServerConfig& config)
{
    std::cout << "[404] Arquivo não encontrado: " << file_path << std::endl;
    
    // Buscar página de erro 404 personalizada
    std::string error_page_404;
    std::map<std::string, std::string>::const_iterator it = config.error_pages.find("404");
    if (it != config.error_pages.end())
        error_page_404 = config.root + it->second;
    
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
    
    if (request.getHeader("Connection") == "close")
        oss << "Connection: close\r\n";
    else if (request.getVersion() == "HTTP/1.0")
        oss << "Connection: close\r\n";
    else
        oss << "Connection: keep-alive\r\n";

    oss << "\r\n";
    oss << final_content;
    response_str = oss.str();
}

void StatusCodes::http405MethodNotAllowed(std::string& response_str, const std::string& file_path)
{
    std::cout << "[405] Método não permitido para: " << file_path << std::endl;

    std::ostringstream oss;
    oss << "HTTP/1.1 405 Method Not Allowed\r\n";
    oss << "Date: " << getCurrentHttpDate() << "\r\n";
    oss << "Server: webserv/1.0\r\n";
    oss << "Content-Type: text/html; charset=UTF-8\r\n";
    oss << "Content-Length: 0\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    response_str = oss.str();
}

void StatusCodes::http408RequestTimeout(std::string& response_str)
{
    std::cout << "[408] Request Timeout" << std::endl;
    
    std::string content = "<html><body><h1>408 Request Timeout</h1><p>The server timed out waiting for the request.</p></body></html>";
    
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

void StatusCodes::http409Conflict(std::string& response_str, const std::string& message)
{
    std::cout << "[409] Conflict: " << message << std::endl;
    
    std::string content = "<html><body><h1>409 Conflict</h1><p>" + message + "</p></body></html>";
    
    std::ostringstream oss;
    oss << "HTTP/1.1 409 Conflict\r\n";
    oss << "Date: " << getCurrentHttpDate() << "\r\n";
    oss << "Server: webserv/1.0\r\n";
    oss << "Content-Type: text/html; charset=UTF-8\r\n";
    oss << "Content-Length: " << content.size() << "\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << content;
    response_str = oss.str();
}

void StatusCodes::http411LengthRequired(std::string& response_str)
{
    std::cout << "[411] Length Required" << std::endl;
    
    std::string content = "<html><body><h1>411 Length Required</h1><p>Content-Length header is required.</p></body></html>";
    
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

void StatusCodes::http413PayloadTooLarge(std::string& response_str)
{
    std::cout << "[413] Payload Too Large" << std::endl;
    
    std::string content = "<html><body><h1>413 Payload Too Large</h1></body></html>";
    
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

void StatusCodes::http414UriTooLong(std::string& response_str)
{
    std::cout << "[414] URI Too Long" << std::endl;
    
    std::string content = "<html><body><h1>414 URI Too Long</h1><p>The requested URI exceeds the maximum length.</p></body></html>";
    
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

void StatusCodes::http415UnsupportedMediaType(std::string& response_str)
{
    std::cout << "[415] Unsupported Media Type" << std::endl;
    
    std::string content = "<html><body><h1>415 Unsupported Media Type</h1></body></html>";
    
    std::ostringstream oss;
    oss << "HTTP/1.1 415 Unsupported Media Type\r\n";
    oss << "Date: " << getCurrentHttpDate() << "\r\n";
    oss << "Server: webserv/1.0\r\n";
    oss << "Content-Type: text/html; charset=UTF-8\r\n";
    oss << "Content-Length: " << content.size() << "\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << content;
    response_str = oss.str();
}

void StatusCodes::http429TooManyRequests(std::string& response_str)
{
    std::cout << "[429] Too Many Requests" << std::endl;
    
    std::string content = "<html><body><h1>429 Too Many Requests</h1><p>Too many requests from this IP. Please try again later.</p></body></html>";
    
    std::ostringstream oss;
    oss << "HTTP/1.1 429 Too Many Requests\r\n";
    oss << "Date: " << getCurrentHttpDate() << "\r\n";
    oss << "Server: webserv/1.0\r\n";
    oss << "Retry-After: 60\r\n";
    oss << "Content-Type: text/html; charset=UTF-8\r\n";
    oss << "Content-Length: " << content.size() << "\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << content;
    response_str = oss.str();
}

// ============================================================================
// ERROS DO SERVIDOR (5xx)
// ============================================================================

void StatusCodes::http500InternalServerError(std::string& response_str, const std::string& message)
{
    std::cout << "[500] Internal Server Error: " << message << std::endl;
    
    std::string content = "<html><body><h1>500 Internal Server Error</h1><p>" + message + "</p></body></html>";
    
    std::ostringstream oss;
    oss << "HTTP/1.1 500 Internal Server Error\r\n";
    oss << "Date: " << getCurrentHttpDate() << "\r\n";
    oss << "Server: webserv/1.0\r\n";
    oss << "Content-Type: text/html; charset=UTF-8\r\n";
    oss << "Content-Length: " << content.size() << "\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << content;
    response_str = oss.str();
}

void StatusCodes::http501NotImplemented(std::string& response_str, const std::string& message)
{
    std::cout << "[501] Not Implemented: " << message << std::endl;
    
    std::string content = "<html><body><h1>501 Not Implemented</h1><p>" + message + "</p></body></html>";
    
    std::ostringstream oss;
    oss << "HTTP/1.1 501 Not Implemented\r\n";
    oss << "Date: " << getCurrentHttpDate() << "\r\n";
    oss << "Server: webserv/1.0\r\n";
    oss << "Content-Type: text/html; charset=UTF-8\r\n";
    oss << "Content-Length: " << content.size() << "\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << content;
    response_str = oss.str();
}

void StatusCodes::http502BadGateway(std::string& response_str, const std::string& message)
{
    std::cout << "[502] Bad Gateway: " << message << std::endl;
    
    std::string content = "<html><body><h1>502 Bad Gateway</h1><p>" + message + "</p></body></html>";
    
    std::ostringstream oss;
    oss << "HTTP/1.1 502 Bad Gateway\r\n";
    oss << "Date: " << getCurrentHttpDate() << "\r\n";
    oss << "Server: webserv/1.0\r\n";
    oss << "Content-Type: text/html; charset=UTF-8\r\n";
    oss << "Content-Length: " << content.size() << "\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << content;
    response_str = oss.str();
}

void StatusCodes::http503ServiceUnavailable(std::string& response_str)
{
    std::cout << "[503] Service Unavailable" << std::endl;
    
    std::string content = "<html><body><h1>503 Service Unavailable</h1><p>The server is temporarily unable to handle the request.</p></body></html>";
    
    std::ostringstream oss;
    oss << "HTTP/1.1 503 Service Unavailable\r\n";
    oss << "Date: " << getCurrentHttpDate() << "\r\n";
    oss << "Server: webserv/1.0\r\n";
    oss << "Retry-After: 120\r\n";
    oss << "Content-Type: text/html; charset=UTF-8\r\n";
    oss << "Content-Length: " << content.size() << "\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << content;
    response_str = oss.str();
}

void StatusCodes::http504GatewayTimeout(std::string& response_str)
{
    std::cout << "[504] Gateway Timeout" << std::endl;
    
    std::string content = "<html><body><h1>504 Gateway Timeout</h1><p>The gateway did not receive a timely response from the upstream server.</p></body></html>";
    
    std::ostringstream oss;
    oss << "HTTP/1.1 504 Gateway Timeout\r\n";
    oss << "Date: " << getCurrentHttpDate() << "\r\n";
    oss << "Server: webserv/1.0\r\n";
    oss << "Content-Type: text/html; charset=UTF-8\r\n";
    oss << "Content-Length: " << content.size() << "\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << content;
    response_str = oss.str();
}

void StatusCodes::http505VersionNotSupported(std::string& response_str)
{
    std::cout << "[505] HTTP Version Not Supported" << std::endl;
    
    std::string content = "<html><body><h1>505 HTTP Version Not Supported</h1><p>The HTTP version used in the request is not supported.</p></body></html>";
    
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
