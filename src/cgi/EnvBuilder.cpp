/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   EnvBuilder.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ajacinto <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/05 12:17:35 by ajacinto          #+#    #+#             */
/*   Updated: 2026/03/05 12:17:37 by ajacinto         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "EnvBuilder.hpp"

#include <algorithm>
#include <cctype>

namespace EnvBuilder {

static std::string toUpper(const std::string& str)
{
    std::string ret = str;
    std::transform(ret.begin(), ret.end(), ret.begin(), ::toupper);
    return ret;
}

std::vector<std::string> build(const HttpRequest& request,
                               const LocationConfig& location,
                               const std::string& scriptPath)
{
    // location might not be used in the simple implementation but kept for
    // future flexibility (e.g. root override, custom vars)
    (void)location;
    std::vector<std::string> env;

     std::cerr << "[ENV] query='" << request.getQuery() << "'\n";
    std::cerr << "[ENV] path='"  << request.getPath()  << "'\n";

    // standard CGI variables
    env.push_back("REDIRECT_STATUS=200");
    env.push_back("REQUEST_METHOD=" + request.getMethod());
    env.push_back("QUERY_STRING=" + request.getQuery());
    env.push_back("SERVER_PROTOCOL=" + request.getVersion());
    env.push_back("GATEWAY_INTERFACE=CGI/1.1");
    env.push_back("SERVER_SOFTWARE=webserv/1.0");
    env.push_back("SCRIPT_FILENAME=" + scriptPath);
    env.push_back("SCRIPT_NAME=" + request.getPath());
    env.push_back("REQUEST_URI=" + request.getUri());
    env.push_back("PATH_INFO=" + request.getPath());
    env.push_back("PATH_TRANSLATED=" + scriptPath);
    if (request.hasHeader("Content-Type"))
        env.push_back("CONTENT_TYPE=" + request.getHeader("Content-Type"));
    if (request.hasHeader("Content-Length"))
        env.push_back("CONTENT_LENGTH=" + request.getHeader("Content-Length"));

    // translate all HTTP headers
    const std::map<std::string, std::string>& hdrs = request.getHeaders();
    for (std::map<std::string, std::string>::const_iterator it = hdrs.begin();
         it != hdrs.end(); ++it)
    {
        std::string name = it->first;
        std::string value = it->second;
        // skip Content-Type/Length (already added)
        if (toUpper(name) == "CONTENT-TYPE" || toUpper(name) == "CONTENT-LENGTH")
            continue;
        // produce HTTP_<HEADER_NAME> with underscores
        for (size_t i = 0; i < name.size(); ++i) {
            if (name[i] == '-')
                name[i] = '_';
        }
        env.push_back("HTTP_" + toUpper(name) + "=" + value);
    }

    // You could add REMOTE_ADDR, SERVER_NAME, etc. if needed.

    return env;
}

} // namespace EnvBuilder
