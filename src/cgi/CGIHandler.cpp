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
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <iostream>

namespace CGIHandler {

bool executeCgi(const HttpRequest& request,
             const std::string& scriptPath,
             const LocationConfig& location,
             std::string& outResponse)
{
    // prepare environment
    std::vector<std::string> envStrings = EnvBuilder::build(request, location, scriptPath);
    std::vector<char*> env;
    env.reserve(envStrings.size() + 1);
    for (size_t i = 0; i < envStrings.size(); ++i)
        env.push_back(const_cast<char*>(envStrings[i].c_str()));
    env.push_back(NULL);

    // prepare argv: [cgi_path, scriptPath, NULL]
    char* argv[3];
    argv[0] = const_cast<char*>(location.cgi_path.c_str());
    argv[1] = const_cast<char*>(scriptPath.c_str());
    argv[2] = NULL;

    int inpipe[2];
    int outpipe[2];
    if (pipe(inpipe) < 0 || pipe(outpipe) < 0)
        return false;

    pid_t pid = fork();
    if (pid < 0)
        return false;
    if (pid == 0)
    {
        // child
        dup2(inpipe[0], STDIN_FILENO);
        dup2(outpipe[1], STDOUT_FILENO);
        // close unused ends
        close(inpipe[1]);
        close(outpipe[0]);

        execve(location.cgi_path.c_str(), argv, env.data());
        // if execve fails
        std::cerr << "[CGI] execve failed: " << strerror(errno) << std::endl;
        _exit(1);
    }

    // parent
    close(inpipe[0]);
    close(outpipe[1]);

    // send request body if present
    if (request.getMethod() == "POST")
    {
        const std::string& body = request.getBody();
        if (!body.empty())
            write(inpipe[1], body.c_str(), body.size());
    }
    close(inpipe[1]);

    // read CGI output
    char buffer[4096];
    ssize_t n;
    outResponse.clear();
    while ((n = read(outpipe[0], buffer, sizeof(buffer))) > 0)
        outResponse.append(buffer, n);

    close(outpipe[0]);

    int status;
    waitpid(pid, &status, 0);

    // parse headers/body
    size_t header_end = outResponse.find("\r\n\r\n");
    std::string headers;
    std::string body;
    if (header_end != std::string::npos)
    {
        headers = outResponse.substr(0, header_end);
        body = outResponse.substr(header_end + 4);
    }
    else
    {
        body = outResponse;
    }

    // build final HTTP response
    std::ostringstream oss;
    // default status
    std::string statusLine = "HTTP/1.1 200 OK\r\n";
    // look for Status: header
    std::istringstream hss(headers);
    std::string line;
    while (std::getline(hss, line))
    {
        if (line.size() >= 7 && line.substr(0,7) == "Status:")
        {
            std::string code = line.substr(7);
            // trim leading spaces
            while (!code.empty() && (code[0] == ' ' || code[0] == '\t'))
                code.erase(0, 1);
            statusLine = "HTTP/1.1 " + code + "\r\n";
            break;
        }
    }
    oss << statusLine;
    // append other headers (except Status)
    std::istringstream hss2(headers);
    while (std::getline(hss2, line))
    {
        if (line.size() >= 7 && line.substr(0,7) == "Status:")
            continue;
        oss << line << "\r\n";
    }
    oss << "\r\n";
    oss << body;

    outResponse = oss.str();
    return true;
}

} // namespace CGIHandler
