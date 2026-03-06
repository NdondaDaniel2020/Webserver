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

namespace CGIHandler
{

bool executeCgi(const HttpRequest &request,
                const std::string &scriptPath,
                const LocationConfig &location,
                std::string &outResponse)
{
    // 1️⃣ Preparar environment
    std::vector<std::string> envStrings = EnvBuilder::build(request, location, scriptPath);
    std::vector<char *> env;
    env.reserve(envStrings.size() + 1);
    for (size_t i = 0; i < envStrings.size(); ++i)
        env.push_back(const_cast<char *>(envStrings[i].c_str()));
    env.push_back(NULL);

    // 2️⃣ Preparar argv: [interpreter, scriptPath, NULL]
    char *argv[3];
    argv[0] = const_cast<char *>(location.cgi_path.c_str());
    argv[1] = const_cast<char *>(scriptPath.c_str());
    argv[2] = NULL;

    // 3️⃣ Criar pipes para stdin e stdout do CGI
    int inpipe[2];
    int outpipe[2];
    if (pipe(inpipe) < 0 || pipe(outpipe) < 0)
        return false;

    // 4️⃣ Criar processo filho
    pid_t pid = fork();
    if (pid < 0)
        return false;

    if (pid == 0)
    {
        // filho
        dup2(inpipe[0], STDIN_FILENO);
        dup2(outpipe[1], STDOUT_FILENO);
        dup2(outpipe[1], STDERR_FILENO);

        close(inpipe[0]);
        close(inpipe[1]);
        close(outpipe[0]);
        close(outpipe[1]);

        execve(location.cgi_path.c_str(), argv, env.data());

        // falha exec
        std::cerr << "[CGI] execve failed: " << strerror(errno) << std::endl;
        _exit(1);
    }

    // 5️⃣ Pai fecha extremidades desnecessárias
    close(inpipe[0]);
    close(outpipe[1]);

    // 6️⃣ Enviar body para CGI se POST
    if (request.getMethod() == "POST")
    {
        const std::string &body = request.getBody();
        if (!body.empty())
            write(inpipe[1], body.c_str(), body.size());
    }
    close(inpipe[1]); // fecha stdin do CGI, sinaliza EOF

    // 7️⃣ Ler output do CGI
    char buffer[4096];
    ssize_t n;
    std::string cgi_output;
    while ((n = read(outpipe[0], buffer, sizeof(buffer))) > 0)
        cgi_output.append(buffer, n);
    close(outpipe[0]);

    // 8️⃣ Esperar processo filho terminar (sem travar indefinitely)
    int status;
    waitpid(pid, &status, 0);

    // 9️⃣ Separar headers do body
    size_t header_end = cgi_output.find("\r\n\r\n");
    std::string headers;
    std::string body;

    if (header_end != std::string::npos)
    {
        headers = cgi_output.substr(0, header_end);
        body = cgi_output.substr(header_end + 4);
    }
    else
    {
        body = cgi_output;
    }

    // 1️⃣0️⃣ Construir status line padrão
    std::string statusLine = "HTTP/1.1 200 OK\r\n";

    std::istringstream hss(headers);
    std::string line;
    while (std::getline(hss, line))
    {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);

        if (line.find("Status:") == 0)
        {
            std::string code = line.substr(7);
            while (!code.empty() && (code[0] == ' ' || code[0] == '\t'))
                code.erase(0, 1);
            statusLine = "HTTP/1.1 " + code + "\r\n";
            break;
        }
    }

    // 1️⃣1️⃣ Montar resposta HTTP final
    std::ostringstream oss;
    oss << statusLine;

    // headers do CGI (exceto Status)
    std::istringstream hss2(headers);
    while (std::getline(hss2, line))
    {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);

        if (line.find("Status:") == 0)
            continue;

        oss << line << "\r\n";
    }

    // adicionar Content-Length e Connection obrigatórios
    oss << "Content-Length: " << body.size() << "\r\n";
    oss << "Connection: keep-alive\r\n";
    oss << "\r\n";
    oss << body;

    outResponse = oss.str();
    return true;
}

} // namespace CGIHandler
