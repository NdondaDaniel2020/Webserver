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
#include <cstring>

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

static void closePipes(int inpipe[2], int outpipe[2])
{
    close(inpipe[0]);
    close(inpipe[1]);
    close(outpipe[0]);
    close(outpipe[1]);
}

bool executeCgi(const HttpRequest  &request,
                const std::string  &scriptPath,
                const LocationConfig &location,
                std::string        &outResponse)
{
    // remove query string
    std::string cleanPath = stripQuery(scriptPath);

    // find interpreter for this extension
    std::string ext = getExtension(cleanPath);

    std::map<std::string, std::string>::const_iterator it =
        location.cgi_handlers.find(ext);

    if (it == location.cgi_handlers.end())
        return false;

    std::string interpreter = it->second;

    // ---------- build environment ----------
    std::vector<std::string> envStrings =
        EnvBuilder::build(request, location, cleanPath);

    std::vector<char *> env;

    for (size_t i = 0; i < envStrings.size(); i++)
        env.push_back(const_cast<char *>(envStrings[i].c_str()));

    env.push_back(NULL);

    // ---------- argv ----------
    char *argv[3];

    argv[0] = const_cast<char *>(interpreter.c_str());
    argv[1] = const_cast<char *>(cleanPath.c_str());
    argv[2] = NULL;

    // ---------- pipes ----------
    int inpipe[2];
    int outpipe[2];

    if (pipe(inpipe) < 0)
        return false;

    if (pipe(outpipe) < 0)
    {
        close(inpipe[0]);
        close(inpipe[1]);
        return false;
    }

    pid_t pid = fork();

    if (pid < 0)
    {
        closePipes(inpipe, outpipe);
        return false;
    }

    // ---------- child ----------
    if (pid == 0)
    {
        dup2(inpipe[0],  STDIN_FILENO);
        dup2(outpipe[1], STDOUT_FILENO);
        dup2(outpipe[1], STDERR_FILENO);

        closePipes(inpipe, outpipe);

        execve(interpreter.c_str(), argv, &env[0]);

        // execve only returns on failure
        perror("execve");
        exit(127);
    }

    // ---------- parent ----------
    close(inpipe[0]);
    close(outpipe[1]);

    // send request body to CGI stdin (POST)
    if (request.getMethod() == "POST")
    {
        const std::string &requestBody = request.getBody();
        write(inpipe[1], requestBody.c_str(), requestBody.size());
    }

    close(inpipe[1]);

    // read CGI output
    char        buffer[4096];
    std::string cgiOutput;
    ssize_t     n;

    while ((n = read(outpipe[0], buffer, sizeof(buffer))) > 0)
        cgiOutput.append(buffer, n);

    close(outpipe[0]);

    int status;
    waitpid(pid, &status, 0);

    // check if CGI exited with error
    if (WIFEXITED(status) && WEXITSTATUS(status) == 127)
        return false;

    // ---------- split headers / body ----------
    size_t pos     = cgiOutput.find("\r\n\r\n");
    size_t sep_len = 4;

    if (pos == std::string::npos)
    {
        pos     = cgiOutput.find("\n\n");
        sep_len = 2;
    }

    std::string cgiHeaders;
    std::string responseBody;

    if (pos != std::string::npos)
    {
        cgiHeaders   = cgiOutput.substr(0, pos);
        responseBody = cgiOutput.substr(pos + sep_len);
    }
    else
    {
        responseBody = cgiOutput;
    }

    // ---------- build HTTP response ----------
    // Check if CGI provided a status line (e.g. "Status: 404 Not Found")
    std::string statusLine = "200 OK";
    std::string finalHeaders;

    std::istringstream headerStream(cgiHeaders);
    std::string        line;

    while (std::getline(headerStream, line))
    {
        // strip trailing \r
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);

        if (line.substr(0, 7) == "Status:")
        {
            statusLine = line.substr(8); // "Status: 404 Not Found" → "404 Not Found"
        }
        else
        {
            finalHeaders += line + "\r\n";
        }
    }

    std::ostringstream response;

    response << "HTTP/1.1 " << statusLine << "\r\n";
    response << finalHeaders;
    response << "Content-Length: " << responseBody.size() << "\r\n";
    response << "Connection: close\r\n";
    response << "\r\n";
    response << responseBody;

    outResponse = response.str();

    return true;
}

} // namespace CGIHandler