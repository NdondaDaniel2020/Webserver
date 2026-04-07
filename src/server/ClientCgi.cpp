/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ClientCgi.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nmatondo <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/29 00:00:00 by nmatondo          #+#    #+#             */
/*   Updated: 2026/03/29 00:00:00 by nmatondo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Client.hpp"
#include "StatusCodes.hpp"
#include "EnvBuilder.hpp"

static char **buildEnvp(const HttpRequest &req, const std::string &script)
{
    std::vector<std::string> envStrings = EnvBuilder::build(req, script);
    char **envp = new char *[envStrings.size() + 1];
    size_t i = 0;
    for (; i < envStrings.size(); ++i)
    {
        envp[i] = new char[envStrings[i].size() + 1];
        std::strcpy(envp[i], envStrings[i].c_str());
    }
    envp[i] = NULL;
    return envp;
}

static void freeEnvp(char **envp)
{
    for (size_t i = 0; envp[i]; ++i)
        delete[] envp[i];
    delete[] envp;
}

void Client::startCgi(const HttpRequest &req, const LocationConfig &loc,
                      const ServerConfig &server_config, int epoll_fd)
{
    std::string error_msg = "Fail CGI";
    
    // Construir path: root + path completo (sem remover loc.path)
    std::string file_path = req.getPath();
    if (file_path[0] == '/')
        file_path = file_path.substr(1);
    
    std::string script_path = loc.root + "/" + file_path;
    std::cout << "[CGI] Script path: " << script_path << std::endl;
    size_t pos = script_path.find('.');
    std::string ext;
    if (pos != std::string::npos)
        ext = script_path.substr(pos);
    else
        ext = "";
    if (!fileExists(script_path))
    {
        StatusCodes::http404NotFound(error_msg, req, script_path, server_config);
        send_buffer = error_msg;
        send_offset = 0;
        state = SENDING_RESPONSE;
        return;
    }

    std::map<std::string, std::string>::const_iterator it = loc.cgi_handlers.find(ext);
    if (it == loc.cgi_handlers.end())
    {
        StatusCodes::http502BadGateway(error_msg, req, "CGI handler not found for extension", server_config);
        send_buffer = error_msg;
        send_offset = 0;
        state = SENDING_RESPONSE;
        return;
    }
    std::string interpreter = it->second;

    if (!fileExists(interpreter))
    {
        std::cerr << "[CGI] Interpreter not found: " << interpreter << std::endl;
        close(cgi.pipe_in[0]);
        close(cgi.pipe_in[1]);
        close(cgi.pipe_out[0]);
        close(cgi.pipe_out[1]);
        cgi.pipe_in[0] = cgi.pipe_in[1] = -1;
        cgi.pipe_out[0] = cgi.pipe_out[1] = -1;
        StatusCodes::http502BadGateway(error_msg, req,
            "CGI interpreter not found: " + interpreter, server_config);
        send_buffer = error_msg;
        send_offset = 0;
        state = SENDING_RESPONSE;
        return;
    }

    if (access(interpreter.c_str(), X_OK) != 0)
    {
        int saved_errno = errno;
        std::cerr << "[CGI] Interpreter not executable: " << interpreter
                  << " - " << strerror(saved_errno) << std::endl;
        close(cgi.pipe_in[0]);
        close(cgi.pipe_in[1]);
        close(cgi.pipe_out[0]);
        close(cgi.pipe_out[1]);
        cgi.pipe_in[0] = cgi.pipe_in[1] = -1;
        cgi.pipe_out[0] = cgi.pipe_out[1] = -1;
        StatusCodes::http502BadGateway(error_msg, req,
            "CGI interpreter not executable: " + interpreter, server_config);
        send_buffer = error_msg;
        send_offset = 0;
        state = SENDING_RESPONSE;
        return;
    }

    cgi.pipe_in[0] = cgi.pipe_in[1] = -1;
    cgi.pipe_out[0] = cgi.pipe_out[1] = -1;

    if (pipe(cgi.pipe_in) < 0)
    {
        int saved_errno = errno;
        std::cerr << "[CGI] pipe(pipe_in) failed: " << strerror(saved_errno) << std::endl;
        cgi.pipe_in[0] = cgi.pipe_in[1] = -1;
        StatusCodes::http502BadGateway(error_msg, req, "Failed to create input pipe", server_config);
        send_buffer = error_msg;
        send_offset = 0;
        state = SENDING_RESPONSE;
        return;
    }

    if (setClosExec(cgi.pipe_in[0]) < 0)
    {
        int saved_errno = errno;
        std::cerr << "[CGI] setClosExec(pipe_in[0]) failed: " << strerror(saved_errno) << std::endl;
        close(cgi.pipe_in[0]);
        close(cgi.pipe_in[1]);
        cgi.pipe_in[0] = cgi.pipe_in[1] = -1;
        StatusCodes::http502BadGateway(error_msg, req, "Failed to set CLOEXEC on input pipe", server_config);
        send_buffer = error_msg;
        send_offset = 0;
        state = SENDING_RESPONSE;
        return;
    }

    if (setClosExec(cgi.pipe_in[1]) < 0)
    {
        int saved_errno = errno;
        std::cerr << "[CGI] setClosExec(pipe_in[1]) failed: " << strerror(saved_errno) << std::endl;
        close(cgi.pipe_in[0]);
        close(cgi.pipe_in[1]);
        cgi.pipe_in[0] = cgi.pipe_in[1] = -1;
        StatusCodes::http502BadGateway(error_msg, req, "Failed to set CLOEXEC on input pipe", server_config);
        send_buffer = error_msg;
        send_offset = 0;
        state = SENDING_RESPONSE;
        return;
    }

    if (pipe(cgi.pipe_out) < 0)
    {
        int saved_errno = errno;
        std::cerr << "[CGI] pipe(pipe_out) failed: " << strerror(saved_errno) << std::endl;
        close(cgi.pipe_in[0]);
        close(cgi.pipe_in[1]);
        cgi.pipe_in[0] = cgi.pipe_in[1] = -1;
        cgi.pipe_out[0] = cgi.pipe_out[1] = -1;
        StatusCodes::http502BadGateway(error_msg, req, "Failed to create output pipe", server_config);
        send_buffer = error_msg;
        send_offset = 0;
        state = SENDING_RESPONSE;
        return;
    }

    if (setClosExec(cgi.pipe_out[0]) < 0)
    {
        int saved_errno = errno;
        std::cerr << "[CGI] setClosExec(pipe_out[0]) failed: " << strerror(saved_errno) << std::endl;
        close(cgi.pipe_in[0]);
        close(cgi.pipe_in[1]);
        close(cgi.pipe_out[0]);
        close(cgi.pipe_out[1]);
        cgi.pipe_in[0] = cgi.pipe_in[1] = -1;
        cgi.pipe_out[0] = cgi.pipe_out[1] = -1;
        StatusCodes::http502BadGateway(error_msg, req, "Failed to set CLOEXEC on output pipe", server_config);
        send_buffer = error_msg;
        send_offset = 0;
        state = SENDING_RESPONSE;
        return;
    }

    if (setClosExec(cgi.pipe_out[1]) < 0)
    {
        int saved_errno = errno;
        std::cerr << "[CGI] setClosExec(pipe_out[1]) failed: " << strerror(saved_errno) << std::endl;
        close(cgi.pipe_in[0]);
        close(cgi.pipe_in[1]);
        close(cgi.pipe_out[0]);
        close(cgi.pipe_out[1]);
        cgi.pipe_in[0] = cgi.pipe_in[1] = -1;
        cgi.pipe_out[0] = cgi.pipe_out[1] = -1;
        StatusCodes::http502BadGateway(error_msg, req, "Failed to set CLOEXEC on output pipe", server_config);
        send_buffer = error_msg;
        send_offset = 0;
        state = SENDING_RESPONSE;
        return;
    }

    fcntl(cgi.pipe_in[1], F_SETFL, O_NONBLOCK);
    fcntl(cgi.pipe_out[0], F_SETFL, O_NONBLOCK);

    char **envp = buildEnvp(req, script_path);
    pid_t pid = fork();
    if (pid < 0)
    {
        int saved_errno = errno;
        std::cerr << "[CGI] fork() failed: " << strerror(saved_errno) << std::endl;
        freeEnvp(envp);
        close(cgi.pipe_in[0]);
        close(cgi.pipe_in[1]);
        close(cgi.pipe_out[0]);
        close(cgi.pipe_out[1]);
        cgi.pipe_in[0] = cgi.pipe_in[1] = -1;
        cgi.pipe_out[0] = cgi.pipe_out[1] = -1;
        StatusCodes::http502BadGateway(error_msg, req, "Failed to fork process", server_config);
        send_buffer = error_msg;
        send_offset = 0;
        state = SENDING_RESPONSE;
        return;
    }

    if (pid == 0)
    {
        dup2(cgi.pipe_in[0], STDIN_FILENO);
        dup2(cgi.pipe_out[1], STDOUT_FILENO);
        int devnull_fd = open("/dev/null", O_WRONLY);
        if (devnull_fd >= 0)
        {
            dup2(devnull_fd, STDERR_FILENO);
            close(devnull_fd);
        }

        close(cgi.pipe_in[1]);
        close(cgi.pipe_out[0]);

        char *argv[3];
        argv[0] = const_cast<char *>(interpreter.c_str());
        argv[1] = const_cast<char *>(script_path.c_str());
        argv[2] = NULL;

        execve(interpreter.c_str(), argv, envp);
        freeEnvp(envp);
        exit(127);
    }

    freeEnvp(envp);

    close(cgi.pipe_in[0]);
    close(cgi.pipe_out[1]);

    cgi.pid = pid;
    cgi.body_written = 0;
    cgi.finished = false;
    cgi.start_time = time(NULL);
    cgi.output = "";
    is_cgi_active = true;

    epoll_event ev;
    ev.events = EPOLLOUT | EPOLLHUP;
    ev.data.fd = cgi.pipe_in[1];
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, cgi.pipe_in[1], &ev) < 0)
    {
        int saved_errno = errno;
        std::cerr << "[CGI] epoll_ctl(pipe_in) failed: " << strerror(saved_errno) << std::endl;
        close(cgi.pipe_in[1]);
        close(cgi.pipe_out[0]);
        cgi.pipe_in[0] = cgi.pipe_in[1] = -1;
        cgi.pipe_out[0] = cgi.pipe_out[1] = -1;
        if (cgi.pid > 0)
        {
            kill(cgi.pid, SIGKILL);
            waitpid(cgi.pid, NULL, 0);
            cgi.pid = -1;
        }
        is_cgi_active = false;
        StatusCodes::http502BadGateway(error_msg, req, "Failed to register input pipe in epoll", server_config);
        send_buffer = error_msg;
        send_offset = 0;
        state = SENDING_RESPONSE;
        return;
    }

    ev.events = EPOLLIN | EPOLLHUP;
    ev.data.fd = cgi.pipe_out[0];
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, cgi.pipe_out[0], &ev) < 0)
    {
        int saved_errno = errno;
        std::cerr << "[CGI] epoll_ctl(pipe_out) failed: " << strerror(saved_errno) << std::endl;
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, cgi.pipe_in[1], NULL);
        close(cgi.pipe_in[1]);
        close(cgi.pipe_out[0]);
        cgi.pipe_in[0] = cgi.pipe_in[1] = -1;
        cgi.pipe_out[0] = cgi.pipe_out[1] = -1;
        if (cgi.pid > 0)
        {
            kill(cgi.pid, SIGKILL);
            waitpid(cgi.pid, NULL, 0);
            cgi.pid = -1;
        }
        is_cgi_active = false;
        StatusCodes::http502BadGateway(error_msg, req, "Failed to register output pipe in epoll", server_config);
        send_buffer = error_msg;
        send_offset = 0;
        state = SENDING_RESPONSE;
        return;
    }
}

void Client::handleCgiStdinWritable(int epoll_fd)
{
    if (!is_cgi_active)
        return;

    const std::string &body = request.getBody();
    if (body.empty())
    {
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, cgi.pipe_in[1], NULL);
        close(cgi.pipe_in[1]);
        cgi.pipe_in[1] = -1;
        return;
    }

    if (cgi.body_written >= body.size())
        return;

    ssize_t w = write(cgi.pipe_in[1], body.c_str() + cgi.body_written,
                      body.size() - cgi.body_written);

    int saved_errno = errno;

    if (w > 0)
    {
        cgi.body_written += static_cast<size_t>(w);
        if (cgi.body_written >= body.size())
        {
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, cgi.pipe_in[1], NULL);
            close(cgi.pipe_in[1]);
            cgi.pipe_in[1] = -1;
        }
        return;
    }

    if (saved_errno == EAGAIN || saved_errno == EWOULDBLOCK)
        return;

    std::cerr << "[CLIENT " << fd << "] CGI stdin write error: " << strerror(saved_errno) << std::endl;

    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, cgi.pipe_in[1], NULL);
    close(cgi.pipe_in[1]);
    cgi.pipe_in[1] = -1;

    if (cgi.pid > 0)
    {
        std::cerr << "[CLIENT " << fd << "] Killing CGI process (PID="
                  << cgi.pid << ") due to write error" << std::endl;
        kill(cgi.pid, SIGKILL);
        int status;
        waitpid(cgi.pid, &status, 0);
        cgi.pid = -1;
    }

    if (cgi.pipe_out[0] >= 0)
    {
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, cgi.pipe_out[0], NULL);
        close(cgi.pipe_out[0]);
        cgi.pipe_out[0] = -1;
    }
    if (cgi.pipe_out[1] >= 0)
    {
        close(cgi.pipe_out[1]);
        cgi.pipe_out[1] = -1;
    }

    std::string error_msg;
    int server_idx = 0;
    const ServerConfig &srv_config = config->getServerConfig(server_idx);
    StatusCodes::http502BadGateway(error_msg, request,
        "CGI process died unexpectedly while reading input", srv_config);

    send_buffer = error_msg;
    send_offset = 0;
    state = SENDING_RESPONSE;
    is_cgi_active = false;

    std::cerr << "[CLIENT " << fd << "] Sent 502 error response, CGI finalized" << std::endl;
}

void Client::handleCgiStdoutReadable(int epoll_fd, std::map<int, Client *> &cgi_fd_map)
{
    if (!is_cgi_active)
        return;

    char buf[BUFFERSIZE];
    ssize_t r = read(cgi.pipe_out[0], buf, sizeof(buf));

    int saved_errno = errno;

    if (r > 0)
    {
        cgi.output.append(buf, r);
        return;
    }

    if (r == 0)
    {
        std::cout << "[CLIENT " << fd << "] CGI stdout EOF" << std::endl;
        if (cgi.pipe_out[0] >= 0)
        {
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, cgi.pipe_out[0], NULL);
            cgi_fd_map.erase(cgi.pipe_out[0]);
            close(cgi.pipe_out[0]);
            cgi.pipe_out[0] = -1;
        }
        finishCgiAndGenerateResponse();
        return;
    }

    if (saved_errno == EAGAIN || saved_errno == EWOULDBLOCK)
        return;

    std::cerr << "[CLIENT " << fd << "] CGI read error: " << strerror(saved_errno) << std::endl;
    if (cgi.pipe_out[0] >= 0)
    {
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, cgi.pipe_out[0], NULL);
        cgi_fd_map.erase(cgi.pipe_out[0]);
        close(cgi.pipe_out[0]);
        cgi.pipe_out[0] = -1;
    }
}

void Client::finishCgiAndGenerateResponse()
{
    int status;
    bool cgi_failed = false;
    std::string failure_reason = "";

    if (cgi.pid > 0)
    {
        pid_t result = waitpid(cgi.pid, &status, WNOHANG);
        if (result == cgi.pid)
        {
            if (WIFEXITED(status))
            {
                int exit_code = WEXITSTATUS(status);
                if (exit_code != 0)
                {
                    cgi_failed = true;
                    if (exit_code == 127)
                        failure_reason = "CGI interpreter not found (exit code 127)";
                    else
                    {
                        std::ostringstream oss;
                        oss << "CGI exited with code " << exit_code;
                        failure_reason = oss.str();
                    }
                    std::cerr << "[CLIENT " << fd << "] " << failure_reason << std::endl;
                }
            }
            else if (WIFSIGNALED(status))
            {
                cgi_failed = true;
                int signal = WTERMSIG(status);
                const char *signal_name = strsignal(signal);

                std::ostringstream oss;
                oss << "CGI killed by signal " << signal;
                if (signal_name)
                    oss << " (" << signal_name << ")";
                failure_reason = oss.str();

                std::cerr << "[CLIENT " << fd << "] " << failure_reason << std::endl;
            }
        }
        cgi.pid = -1;
    }

    is_cgi_active = false;

    if (cgi_failed)
    {
        std::string error_msg;
        StatusCodes::http502BadGateway(error_msg, request, failure_reason,
                                       config->getServerConfig(server_index));
        send_buffer = error_msg;
        send_offset = 0;
        state = SENDING_RESPONSE;
        return;
    }

    size_t pos = cgi.output.find("\r\n\r\n");
    size_t header_end_len = 4;
    if (pos == std::string::npos)
    {
        pos = cgi.output.find("\n\n");
        header_end_len = 2;
    }

    std::string cgi_headers = (pos != std::string::npos)
        ? cgi.output.substr(0, pos) : "";
    std::string cgi_body = (pos != std::string::npos)
        ? cgi.output.substr(pos + header_end_len) : cgi.output;

    std::string status_line = "200 OK";
    std::string extra_headers;

    std::istringstream iss(cgi_headers);
    std::string line;
    while (std::getline(iss, line))
    {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);

        if (line.empty())
            continue;

        if (line.find("Status: ") == 0)
        {
            status_line = line.substr(8);
            if (!status_line.empty() && status_line[status_line.size() - 1] == '\r')
                status_line.erase(status_line.size() - 1);
        }
        else
            extra_headers += line + "\r\n";
    }

    StatusCodes::http200CgiResponse(send_buffer, status_line, extra_headers,
                                    cgi_body, keep_alive);
    send_offset = 0;
    state = SENDING_RESPONSE;
}
