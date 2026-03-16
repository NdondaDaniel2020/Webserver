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
    if (pos == std::string::npos) return "";
    return path.substr(pos);
}
 
static std::string stripQuery(const std::string &uri)
{
    size_t q = uri.find('?');
    if (q != std::string::npos) return uri.substr(0, q);
    return uri;
}
 
static void closeAll(int inpipe[2], int outpipe[2])
{
    close(inpipe[0]);
    close(inpipe[1]);
    close(outpipe[0]);
    close(outpipe[1]);
}
 
// ----------------------------------------------------------------
// FIX 1: usa select() para escrever stdin e ler stdout
//         simultaneamente, evitando deadlock em bodies grandes.
// FIX 2: waitpid bloqueante (flag 0) para garantir que o filho
//         terminou de escrever antes de considerarmos o output.
// FIX 3: loop de leitura trata EAGAIN correctamente — sem
//         non-blocking no pipe de leitura, usa timeout via select.
// FIX 4: parse de "Status:" com trim correcto.
// FIX 5: verificação do exit-code do filho (127 = execve falhou).
// ----------------------------------------------------------------
bool executeCgi(const HttpRequest   &request,
                const std::string   &scriptPath,
                const LocationConfig &location,
                std::string         &outResponse
                )
{
    int                  timeoutSecs = 10;
    std::string cleanPath = stripQuery(scriptPath);
    std::string ext       = getExtension(cleanPath);
 
    std::map<std::string, std::string>::const_iterator it =
        location.cgi_handlers.find(ext);
    if (it == location.cgi_handlers.end()) return false;
 
    std::string interpreter = it->second;
 
    std::vector<std::string> envStrings =
        EnvBuilder::build(request, location, cleanPath);
 
    std::vector<char *> env;
    for (size_t i = 0; i < envStrings.size(); i++)
        env.push_back(const_cast<char *>(envStrings[i].c_str()));
    env.push_back(NULL);
 
    char *argv[3];
    argv[0] = const_cast<char *>(interpreter.c_str());
    argv[1] = const_cast<char *>(cleanPath.c_str());
    argv[2] = NULL;
 
    int inpipe[2], outpipe[2];
 
    if (pipe(inpipe) < 0) return false;
    if (pipe(outpipe) < 0) {
        close(inpipe[0]); close(inpipe[1]);
        return false;
    }
 
    // FIX 3 – NÃO tornar outpipe[0] non-blocking;
    //          usamos select() com timeout em vez disso.
 
    pid_t pid = fork();
    if (pid < 0) { closeAll(inpipe, outpipe); return false; }
 
    // ------- FILHO -------
    if (pid == 0) {
        dup2(inpipe[0],  STDIN_FILENO);
        dup2(outpipe[1], STDOUT_FILENO);
        dup2(outpipe[1], STDERR_FILENO);
        closeAll(inpipe, outpipe);
        execve(interpreter.c_str(), argv, &env[0]);
        exit(127);
    }
 
    // ------- PAI -------
    close(inpipe[0]);
    close(outpipe[1]);
 
    const std::string &requestBody = request.getBody();
    size_t bodyWritten = 0;
    bool   writesDone  = requestBody.empty();
 
    std::string cgiOutput;
    char buffer[4096];
 
    // FIX 1 – select() para escrever e ler em paralelo.
    while (true)
    {
        fd_set rfds, wfds;
        FD_ZERO(&rfds); FD_ZERO(&wfds);
 
        FD_SET(outpipe[0], &rfds);
 
        if (!writesDone)
            FD_SET(inpipe[1], &wfds);
 
        int maxfd = (inpipe[1] > outpipe[0] ? inpipe[1] : outpipe[0]) + 1;
 
        struct timeval tv;
        tv.tv_sec  = timeoutSecs;
        tv.tv_usec = 0;
 
        int ret = select(maxfd, &rfds, &wfds, NULL, &tv);
 
        if (ret < 0) {
            // erro real — mata filho e sai
            kill(pid, SIGKILL);
            break;
        }
        if (ret == 0) {
            // timeout — mata filho e sai
            kill(pid, SIGKILL);
            break;
        }
 
        // Pode escrever no stdin do filho?
        if (!writesDone && FD_ISSET(inpipe[1], &wfds)) {
            ssize_t w = write(inpipe[1],
                              requestBody.c_str() + bodyWritten,
                              requestBody.size()  - bodyWritten);
            if (w > 0) {
                bodyWritten += (size_t)w;
                if (bodyWritten >= requestBody.size()) {
                    close(inpipe[1]);
                    writesDone = true;
                }
            } else {
                // filho fechou stdin antes — tudo bem
                close(inpipe[1]);
                writesDone = true;
            }
        }
 
        // Há dados no stdout do filho?
        if (FD_ISSET(outpipe[0], &rfds)) {
            ssize_t n = read(outpipe[0], buffer, sizeof(buffer));
            if (n > 0) {
                cgiOutput.append(buffer, n);
            } else {
                // n == 0 → EOF; n < 0 → erro
                break;
            }
        }
    }
 
    // Drena o resto após EOF (caso o select tenha saído pelo write side)
    {
        // Coloca em non-blocking apenas para drenar o restante
        fcntl(outpipe[0], F_SETFL, O_NONBLOCK);
        ssize_t n;
        while ((n = read(outpipe[0], buffer, sizeof(buffer))) > 0)
            cgiOutput.append(buffer, n);
    }
 
    close(outpipe[0]);
    if (!writesDone) close(inpipe[1]);
 
    // FIX 2 – waitpid nao bloqueante: filho normal ja terminou quando
    //          o pipe fechou (EOF); se ainda vivo (timeout/SIGKILL),
    //          da 50ms ao kernel e tenta WNOHANG; so bloqueia em
    //          ultimo caso para evitar zombie.
    int status = 0;
    usleep(50000); // 50ms suficiente para o kernel limpar apos SIGKILL
    if (waitpid(pid, &status, WNOHANG) == 0) {
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
    }
 
    // FIX 5 – se execve falhou (exit 127), retorna false
    if (WIFEXITED(status) && WEXITSTATUS(status) == 127)
        return false;
 
    if (cgiOutput.empty()) return false;
 
    // ---------- split headers / body ----------
    size_t pos     = cgiOutput.find("\r\n\r\n");
    size_t sep_len = 4;
 
    if (pos == std::string::npos) {
        pos     = cgiOutput.find("\n\n");
        sep_len = 2;
    }
 
    std::string cgiHeaders;
    std::string responseBody;
 
    if (pos != std::string::npos) {
        cgiHeaders   = cgiOutput.substr(0, pos);
        responseBody = cgiOutput.substr(pos + sep_len);
    } else {
        responseBody = cgiOutput;
    }
 
    std::string statusLine  = "200 OK";
    std::string finalHeaders;
 
    std::istringstream headerStream(cgiHeaders);
    std::string line;
 
    while (std::getline(headerStream, line)) {
        // remove \r final se existir
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
 
        // FIX 4 – trim correcto de "Status: "
        if (line.size() >= 8 && line.substr(0, 8) == "Status: ")
            statusLine = line.substr(8);
        else if (!line.empty())
            finalHeaders += line + "\r\n";
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
 