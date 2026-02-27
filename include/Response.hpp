/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Response.hpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nmatondo <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/10/30 13:40:23 by nmatondo          #+#    #+#             */
/*   Updated: 2026/02/27 11:00:34 by nmatondo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef RESPONSE_HPP
# define RESPONSE_HPP

# include <string>
# include <sstream>
# include <dirent.h>
# include "FileUtils.hpp"
# include "ConfigParser.hpp"
# include "HttpRequest.hpp"

class Response
{
    private:
        ServerConfig config;
        std::string response_str;
        std::vector<std::string> allowed_extensions;

    public:
        ~Response();
        Response(const Response& other);
        Response& operator=(const Response& other);
        Response(const HttpRequest& request, const ServerConfig& config);
        
        std::string getResponseHttp();
        
    private:
        void buildHttpResponse(const HttpRequest& request);
        void httpFileFound200(const HttpRequest& request, const std::string& content, const std::string& file_path);
        void httpFileNotFound404(const HttpRequest& request, const std::string& content, const std::string& file_path);
        void methodNotAllowed405(const std::string& file_path);
        void httpForbidden403(const std::string& file_path);
        void httpCreated201(const std::string& location, const std::string& message);
        void httpPayloadTooLarge413();
        void httpUnsupportedMediaType415();
        void httpBadRequest400(const std::string& message);

        
        void methodGet(const HttpRequest& request, const std::string& file_path);
        void methodPost(const HttpRequest& request, const std::string& file_path);        
        void methodDelete(const HttpRequest& request, const std::string& file_path);

        
        void multipartFormData(const HttpRequest& request, const std::string& file_path, const std::string& content_type);
        
        
        const LocationConfig* findMatchingLocation(const std::string& uri) const;
        bool validateAllowedMethod(const HttpRequest& request);
        std::string getUploadDir(const HttpRequest& request, const std::string& file_path);
        void generateDirectoryListing(const HttpRequest& request, const std::string& dir_path, const std::string& uri);
        void handleRedirect(int code, const std::string& url);
};

#endif