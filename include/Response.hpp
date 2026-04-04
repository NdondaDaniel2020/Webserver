/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Response.hpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nmatondo <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/10/30 13:40:23 by nmatondo          #+#    #+#             */
/*   Updated: 2026/04/05 00:25:00 by nmatondo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef RESPONSE_HPP
# define RESPONSE_HPP

# include <string>
# include <sstream>
# include <dirent.h>
# include <cerrno>
# include <cstring>
# include "FileUtils.hpp"
# include "ConfigParser.hpp"
# include "HttpRequest.hpp"
# include "StatusCodes.hpp"
// # include "CGIHandler.hpp"

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
        
        void methodGet(const HttpRequest& request, const std::string& file_path);
        void methodPost(const HttpRequest& request);
        void methodDelete(const HttpRequest& request, const std::string& file_path);
        
        void multipartFormData(const HttpRequest& request, const std::string& content_type);
        void applicationOctetStream(const HttpRequest& request);
        
        void generateDirectoryListing(const HttpRequest& request, const std::string& dir_path, const std::string& uri);
};

#endif