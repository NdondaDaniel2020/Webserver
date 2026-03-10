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

    bool executeCgi(const HttpRequest &request,
                    const std::string &scriptPath,
                    const LocationConfig &location,
                    std::string &outResponse)
    {
        // ---------- build environment ----------
        std::vector<std::string> envStrings = EnvBuilder::build(request, location, scriptPath);

        std::vector<char *> env;
        for (size_t i = 0; i < envStrings.size(); i++)
            env.push_back(const_cast<char *>(envStrings[i].c_str()));
        env.push_back(NULL);

        // ---------- argv ----------
        char *argv[3];
        argv[0] = const_cast<char *>(location.cgi_path.c_str());
        argv[1] = const_cast<char *>(scriptPath.c_str());
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
            // CHILD PROCESS

            dup2(inpipe[0], STDIN_FILENO);
            dup2(outpipe[1], STDOUT_FILENO);
            dup2(outpipe[1], STDERR_FILENO);

            close(inpipe[1]);
            close(outpipe[0]);

            execve(location.cgi_path.c_str(), argv, &env[0]);

            std::cerr << "CGI execve failed\n";
            exit(1);
        }

        // ---------- PARENT PROCESS ----------

        close(inpipe[0]);
        close(outpipe[1]);

        // enviar body se POST
        if (request.getMethod() == "POST")
        {
            const std::string &body = request.getBody();
            write(inpipe[1], body.c_str(), body.size());
        }

        close(inpipe[1]);

        // ---------- ler output CGI ----------

        char buffer[4096];
        std::string cgi_output;
        ssize_t n;

        while ((n = read(outpipe[0], buffer, sizeof(buffer))) > 0)
            cgi_output.append(buffer, n);

        close(outpipe[0]);

        int status;
        waitpid(pid, &status, 0);

        // ---------- separar headers/body ----------

        size_t pos = cgi_output.find("\r\n\r\n");

        std::string headers;
        std::string body;

        if (pos != std::string::npos)
        {
            headers = cgi_output.substr(0, pos);
            body = cgi_output.substr(pos + 4);
        }
        else
            body = cgi_output;

        // ---------- montar resposta HTTP ----------

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