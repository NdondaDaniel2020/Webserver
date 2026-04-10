/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Response.cpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nmatondo <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/03 10:05:33 by nmatondo          #+#    #+#             */
/*   Updated: 2026/04/10 12:23:04 by nmatondo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Response.hpp"
#include "ResponseHelpers.hpp"
#include <algorithm>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <sys/stat.h>

Response::Response(const HttpRequest &request, const ServerConfig &config) : config(config)
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

void Response::buildHttpResponse(const HttpRequest &request)
{
    std::string uri = sanitizePath(request.getUri());
    std::string root = this->config.root;
    const LocationConfig *location = findMatchingLocation(this->config, request.getUri());

    if (!location)
        return StatusCodes::http404NotFound(this->response_str, request, request.getUri(), this->config);

    if (location && !location->root.empty())
        root = location->root;

    if (location && location->redirect_code > 0)
        return handleRedirect(this->response_str, location, request, this->config);

    std::string file_path = root + removeLocationInUri(uri, location);
    if (request.getMethod() == "GET")
        methodGet(request, file_path);
    else if (request.getMethod() == "POST")
        methodPost(request);
    else if (request.getMethod() == "DELETE")
        methodDelete(request, file_path);
    else
        StatusCodes::http405MethodNotAllowed(this->response_str, request, request.getUri(), this->config);
}
