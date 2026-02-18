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

struct HttpRequest
{
    std::string path;
    std::string method;
    std::string version;
};

class Response
{
    private:
        std::string root;
        std::string response_str;
        std::map<std::string, std::string> error_pages;

    public:
        ~Response();
        std::string getResponseHttp();
        Response(const Response& other);
        Response(const std::string& request);
        Response& operator=(const Response& other);

    private:
        void buildHttpResponse(const HttpRequest& request);
        HttpRequest parseHttpRequest(const std::string& raw_request);
        void httpFileFound(const std::string& content, const std::string& file_path);
        void httpFileNotFound(const std::string& content, const std::string& file_path);
};

#endif