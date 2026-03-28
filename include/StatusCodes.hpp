/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   StatusCodes.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nmatondo <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/28 11:30:00 by nmatondo          #+#    #+#             */
/*   Updated: 2026/03/28 20:05:15 by nmatondo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef STATUSCODES_HPP
# define STATUSCODES_HPP

# include <map>
# include <string>
# include <sstream>
# include "FileUtils.hpp"
# include "HttpRequest.hpp"
# include "ConfigParser.hpp"

class StatusCodes
{
    public:
        static std::string findErrorPage(const std::map<std::string, std::string>& error_pages, const std::string& code);
        // Respostas de Sucesso (2xx)
        static void http200Ok(std::string& response_str, const std::string& message);
        static void http200FileFound(std::string& response_str, const HttpRequest& request, const std::string& content, const std::string& file_path, const ServerConfig& config);
        static void http200CgiResponse(std::string& response_str, const std::string& status_line, const std::string& headers, const std::string& body, bool keep_alive);
        static void http201Created(std::string& response_str, const std::string& location, const std::string& message, const HttpRequest& request, const ServerConfig& config);
        static void http204NoContent(std::string& response_str);

        // Respostas de Redirecionamento (3xx)
        static void http301MovedPermanently(std::string& response_str, const std::string& location, const HttpRequest& request, const ServerConfig& config);
        static void http302Found(std::string& response_str, const std::string& location, const HttpRequest& request, const ServerConfig& config);
        static void http304NotModified(std::string& response_str, const HttpRequest& request, const ServerConfig& config);

        // Erros do Cliente (4xx)
        static void http400BadRequest(std::string& response_str, const std::string& message, const ServerConfig& config, const HttpRequest& request);
        static void http401Unauthorized(std::string& response_str, const HttpRequest& request, const std::string& message, const ServerConfig& config);
        static void http403Forbidden(std::string& response_str, const HttpRequest& request, const std::string& file_path, const ServerConfig& config);
        static void http404NotFound(std::string& response_str, const HttpRequest& request, const std::string& file_path, const ServerConfig& config);
        static void http405MethodNotAllowed(std::string& response_str, const HttpRequest& request, const std::string& file_path, const ServerConfig& config);
        static void http408RequestTimeout(std::string& response_str, const ServerConfig& config, const HttpRequest& request);
        static void http409Conflict(std::string& response_str, const HttpRequest& request, const std::string& message, const ServerConfig& config);
        static void http411LengthRequired(std::string& response_str, const ServerConfig& config, const HttpRequest& request);
        static void http413PayloadTooLarge(std::string& response_str, const ServerConfig& config, const HttpRequest& request);
        static void http414UriTooLong(std::string& response_str, const ServerConfig& config, const HttpRequest& request);
        static void http415UnsupportedMediaType(std::string& response_str, const HttpRequest& request, const ServerConfig& config);
        static void http429TooManyRequests(std::string& response_str, const HttpRequest& request, const ServerConfig& config);

        // Erros do Servidor (5xx)
        static void http500InternalServerError(std::string& response_str, const HttpRequest& request, const std::string& message, const ServerConfig& config);
        static void http501NotImplemented(std::string& response_str, const HttpRequest& request, const std::string& message, const ServerConfig& config);
        static void http502BadGateway(std::string& response_str, const HttpRequest& request, const std::string& message, const ServerConfig& config);
        static void http503ServiceUnavailable(std::string& response_str, const HttpRequest& request, const ServerConfig& config);
        static void http504GatewayTimeout(std::string& response_str, const HttpRequest& request, const ServerConfig& config);
        static void http505VersionNotSupported(std::string& response_str, const ServerConfig& config, const HttpRequest& request);
};

#endif
