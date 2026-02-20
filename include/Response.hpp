/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Response.hpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nmatondo <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/10/30 13:40:23 by nmatondo          #+#    #+#             */
/*   Updated: 2026/02/03 11:55:43 by nmatondo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef RESPONSE_HPP
# define RESPONSE_HPP

# include <string>
# include <sstream>
# include "FileUtils.hpp"
# include "ConfigParser.hpp"
# include "HttpRequest.hpp"

class Response
{
    private:
        ServerConfig config;
        std::string response_str;

    public:
        ~Response();
        Response(const Response& other);
        Response& operator=(const Response& other);
        Response(const HttpRequest& request, const ServerConfig& config);
        
        std::string getResponseHttp();

    private:
        void buildHttpResponse(const HttpRequest& request);
        void httpFileFound(const std::string& content, const std::string& file_path);
        void httpFileNotFound(const std::string& content, const std::string& file_path);
};

#endif