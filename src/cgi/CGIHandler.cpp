/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   CGIHandler.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ajacinto <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/05 12:17:14 by ajacinto          #+#    #+#             */
/*   Updated: 2026/03/05 12:17:18 by ajacinto         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "CGIHandler.hpp"
#include "EnvBuilder.hpp"

#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sstream>
#include <iostream>

namespace CGIHandler
{

static std::string getExtension(const std::string &path)
{
    size_t pos = path.rfind('.');

    if (pos == std::string::npos)
        return "";

    return path.substr(pos);
}

static std::string stripQuery(const std::string &uri)
{
    size_t q = uri.find('?');

    if (q != std::string::npos)
        return uri.substr(0, q);

    return uri;
}

bool executeCgi(const HttpRequest &request,
                            const std::string &scriptPath,
                            const LocationConfig &location,
                            std::string &outResponse)
{
    // remover query string
    std::string cleanPath = stripQuery(scriptPath);

    // descobrir extensão
    std::string ext = getExtension(cleanPath);

    std::map<std::string,std::string>::const_iterator it =
        location.cgi_handlers.find(ext);

    if (it == location.cgi_handlers.end())
        return false;

    std::string interpreter = it->second;

    // ---------- build environment ----------
    std::vector<std::string> envStrings =
        EnvBuilder::build(request, location, cleanPath);

    std::vector<char*> env;

    for (size_t i = 0; i < envStrings.size(); i++)
        env.push_back(const_cast<char*>(envStrings[i].c_str()));

    env.push_back(NULL);

    // ---------- argv ----------
    char *argv[3];

    argv[0] = const_cast<char*>(interpreter.c_str());
    argv[1] = const_cast<char*>(cleanPath.c_str());
    argv[2] = NULL;

    // ---------- pipes ----------
    int inpipe[2];
    int outpipe[2];

    if (pipe(inpipe) < 0 || pipe(outpipe) < 0)
        return false;

    pid_t pid = fork();

    if (pid < 0)
        return false;

    if (pid == 0)
    {
        dup2(inpipe[0], STDIN_FILENO);
        dup2(outpipe[1], STDOUT_FILENO);
        dup2(outpipe[1], STDERR_FILENO);

        close(inpipe[0]);
        close(inpipe[1]);
        close(outpipe[0]);
        close(outpipe[1]);

        execve(interpreter.c_str(), argv, &env[0]);

        exit(1);
    }

    // ---------- parent ----------
    close(inpipe[0]);
    close(outpipe[1]);

    if (request.getMethod() == "POST")
    {
        const std::string &body = request.getBody();
        write(inpipe[1], body.c_str(), body.size());
    }

    close(inpipe[1]);

    char buffer[4096];
    std::string cgi_output;
    ssize_t n;

    while ((n = read(outpipe[0], buffer, sizeof(buffer))) > 0)
        cgi_output.append(buffer, n);

    close(outpipe[0]);

    int status;
    waitpid(pid, &status, 0);

    // ---------- separar headers ----------
    size_t pos = cgi_output.find("\r\n\r\n");
    size_t sep_len = 4;

    if (pos == std::string::npos)
    {
        pos = cgi_output.find("\n\n");
        sep_len = 2;
    }

    std::string headers;
    std::string body;

    if (pos != std::string::npos)
    {
        headers = cgi_output.substr(0, pos);
        body = cgi_output.substr(pos + sep_len);
    }
    else
        body = cgi_output;

    std::ostringstream response;

    response << "HTTP/1.1 200 OK\r\n";
    response << headers << "\r\n";
    response << "Content-Length: " << body.size() << "\r\n";
    response << "Connection: close\r\n";
    response << "\r\n";
    response << body;

    outResponse = response.str();

    return true;
}

}