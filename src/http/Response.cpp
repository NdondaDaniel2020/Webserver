/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Response.cpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nmatondo <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/03 10:05:33 by nmatondo          #+#    #+#             */
/*   Updated: 2026/03/28 19:19:05 by nmatondo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../../include/Response.hpp"
#include "../../include/ResponseHelpers.hpp"
#include <algorithm>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <sys/stat.h>

Response::Response(const HttpRequest &request, const ServerConfig &config) : config(config)
{
    this->allowed_extensions.push_back(".jpg");
    this->allowed_extensions.push_back(".jpeg");
    this->allowed_extensions.push_back(".png");
    this->allowed_extensions.push_back(".gif");
    this->allowed_extensions.push_back(".pdf");
    this->allowed_extensions.push_back(".txt");
    this->allowed_extensions.push_back(".doc");
    this->allowed_extensions.push_back(".docx");
    this->allowed_extensions.push_back(".zip");
    this->allowed_extensions.push_back(".mp4");
    this->allowed_extensions.push_back(".mp3");
    this->allowed_extensions.push_back(".log");

    this->protected_files.push_back("index.html");

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
        this->protected_files = other.protected_files;
        this->allowed_extensions = other.allowed_extensions;
    }
    return *this;
}

std::string Response::getResponseHttp()
{
    return this->response_str;
}

void Response::buildHttpResponse(const HttpRequest &request)
{
    std::string uri = sanitizePath(request.getUri());
    std::string root = this->config.root;
    const LocationConfig *location = findMatchingLocation(this->config, request.getUri());

    if (location && !location->root.empty())
        root = location->root;

    std::string file_path = root + removeLocationInUri(uri, location);

    if (!validateAllowedMethod(this->config, request))
    {
        std::cout << "[405] Método " << request.getMethod() << " não permitido para: " << request.getUri() << std::endl;
        return StatusCodes::http405MethodNotAllowed(this->response_str, request, file_path, this->config);
    }

    if (request.getMethod() == "GET")
        methodGet(request, file_path);
    else if (request.getMethod() == "POST")
        methodPost(request);
    else if (request.getMethod() == "DELETE")
        methodDelete(request, file_path);
    else
        StatusCodes::http405MethodNotAllowed(this->response_str, request, file_path, this->config);
}


