/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nmatondo <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/19 11:33:45 by nmatondo          #+#    #+#             */
/*   Updated: 2026/03/25 12:01:04 by nmatondo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Client.hpp"
#include "StatusCodes.hpp"
#include "EnvBuilder.hpp" // para gerar envp correto para CGI

Client::Client(int fd, const ConfigParser *config, int server_index)
    : fd(fd),
      state(READING_HEADERS),
      last_activity(0), // ← corrigido
      recv_buffer(),
      send_buffer(),
      send_offset(0),
      request(),
      response(NULL),
      keep_alive(false),
      content_length(0),
      headers_end_pos(0),
      config(config),
      cgi(),
      is_cgi_active(false),
      is_chunked(false),
      server_index(server_index)
{
    updateLastActivity();
}

Client::~Client()
{
    if (response)
        delete response;

    // ✅ RAII: Cliente limpa SEUS recursos (não depende de Server)
    // Defensive programming: Se Server esquecer cleanupCgiIfActive(), não vaza
    
    if (cgi.pid > 0)
    {
        std::cerr << "[WARNING] ~Client() cleaning up active CGI (pid=" << cgi.pid 
                  << "). Server should have called cleanupCgiIfActive()." << std::endl;
        kill(cgi.pid, SIGKILL);
        waitpid(cgi.pid, NULL, WNOHANG);
        cgi.pid = -1;
    }

    // Fechar pipes (kernel remove do epoll automaticamente ao fechar FD)
    if (cgi.pipe_in[1] >= 0)
    {
        close(cgi.pipe_in[1]);
        cgi.pipe_in[1] = -1;
    }

    if (cgi.pipe_out[0] >= 0)
    {
        close(cgi.pipe_out[0]);
        cgi.pipe_out[0] = -1;
    }

    // Zerar flags
    is_cgi_active = false;
    is_chunked = false;
}

void Client::cleanupCgiIfActive(int epoll_fd)
{
    if (!is_cgi_active)
        return;

    if (cgi.pid > 0)
    {
        kill(cgi.pid, SIGKILL);
        waitpid(cgi.pid, NULL, WNOHANG);
        cgi.pid = -1;
    }

    if (cgi.pipe_in[1] >= 0)
    {
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, cgi.pipe_in[1], NULL);
        close(cgi.pipe_in[1]);
        cgi.pipe_in[1] = -1;
    }

    if (cgi.pipe_out[0] >= 0)
    {
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, cgi.pipe_out[0], NULL);
        close(cgi.pipe_out[0]);
        cgi.pipe_out[0] = -1;
    }

    is_cgi_active = false;

    std::cout << "[CGI CLEANUP] Processo filho do cliente fd=" << fd
              << " terminado (SIGKILL enviado)" << std::endl;
}

int Client::getServerIndex()
{
    return server_index;
}

Client::CgiState &Client::getCgiState()
{
     return cgi;
}

// ========== Getters ==========

int Client::getFd() const
{
    return fd;
}

int Client::getCgiOutFd() const
{
    return cgi.pipe_out[0];
}

int Client::getCgiInFd() const
{
    return cgi.pipe_in[1];
}

Client::State Client::getState() const
{
    return state;
}

bool Client::isKeepAlive() const
{
    return keep_alive;
}

bool Client::isDone() const
{
    return state == DONE;
}

time_t Client::getLastActivity() const
{
    return last_activity;
}

time_t    Client::getCgiStartTime() const
{
    return cgi.start_time;
}

bool Client::hasDataToSend() const
{
    return !send_buffer.empty() && send_offset < send_buffer.size();
}

// ========== Recepção de dados ==========

void Client::appendRecvData(const char *data, size_t len)
{
    recv_buffer.append(data, len);
    updateLastActivity();
}

bool  Client::unchunkBody(std::string& out)
{
    const std::string& buffer = recv_buffer;
    size_t pos = headers_end_pos;

    while (pos < buffer.size())
    {
        size_t control_f = buffer.find("\r\n", pos);
        if (control_f == std::string::npos)
            return false;
        std::string value_hex = buffer.substr(pos, control_f - pos);
        size_t chunk_size = 0;
        std::stringstream iss(value_hex);
        iss >> std::hex >> chunk_size;
        pos = control_f + 2;
        if (chunk_size == 0)
            return true;
        if (pos + chunk_size + 2 > buffer.size())
            return false;
        out += buffer.substr(pos, chunk_size);
        pos += chunk_size + 2;
    }
    return false;
}

bool Client::isRequestComplete()
{
    if (state == READING_HEADERS)
    {
        if (findHeadersEnd())
        {
            parseHeaders();

            // Se parseHeaders detectou erro 413, marcar como completo
            if (state == ERROR_413)
                return true;
            if (is_chunked)
                state = READING_BODY;
            // Se não tem body (GET, POST, DELETE) e Content-Length: 0
            else if ((request.getMethod() == "GET" ||
                 request.getMethod() == "POST" ||
                 request.getMethod() == "DELETE") &&
                content_length == 0)
            {
                state = PROCESSING;
                return true;
            }
            else
                state = READING_BODY;
        }
    }

    if (state == READING_BODY)
    {
        if (is_chunked)
        {
            std::string unchunked_body;

            if (unchunkBody(unchunked_body))
            {
                size_t max_size = config->getServerConfig(getServerIndex()).client_max_body_size;
                if (max_size > 0 && unchunked_body.size() > max_size)
                {
                    request.setBody(unchunked_body);
                    state = ERROR_413;
                }
                else
                {
                    request.setBody(unchunked_body);
                    state = PROCESSING;
                }
                return true;
            }
        }
        else
        {
            if (checkBodyComplete())
            {
                state = PROCESSING;
                return true;
            }
        }
    }

    return false;
}

bool Client::findHeadersEnd()
{
    size_t pos = recv_buffer.find("\r\n\r\n");
    if (pos != std::string::npos)
    {
        headers_end_pos = pos + 4; // +4 para pular o \r\n\r\n
        return true;
    }
    return false;
}

void Client::parseHeaders()
{
    // Parse apenas os headers (até headers_end_pos)
    std::string headers_only = recv_buffer.substr(0, headers_end_pos);
    request = HttpRequest::parse(headers_only);

    // Verificar Content-Length
    if (request.hasHeader("Content-Length"))
    {
        std::istringstream iss(request.getHeader("Content-Length"));
        iss >> content_length;

        // ✅ VALIDAR Content-Length ANTES de receber o body
        if (config)
        {
            size_t max_size = config->getServerConfig(getServerIndex()).client_max_body_size;
            if (max_size > 0 && content_length > max_size)
            {
                std::cout << "[413] Content-Length (" << content_length
                          << ") excede limite (" << max_size
                          << ") - Rejeitando ANTES de receber body" << std::endl;
                state = ERROR_413;
                return;
            }
        }
    }

    // Verificar keep-alive
    if (request.hasHeader("Connection"))
    {
        std::string conn = request.getHeader("Connection");
        keep_alive = (conn == "keep-alive" || conn == "Keep-Alive");
    }
    else
    {
        // HTTP/1.1 default é keep-alive
        keep_alive = (request.getVersion() == "HTTP/1.1");
    }
    if (request.hasHeader("Transfer-Encoding"))
    {
        std::string transf_enco = request.getHeader("Transfer-Encoding");
        if (transf_enco == "chunked")
        {
            is_chunked = true;
            content_length = 0;
        }
    }
}

bool Client::checkBodyComplete()
{
    size_t body_received = recv_buffer.size() - headers_end_pos;
    return body_received >= content_length;
}

// ========== Processamento ==========

static const LocationConfig *findMatchingLocation(const ServerConfig &config, const std::string &uri)
{
    const LocationConfig *best_match = NULL;
    size_t best_match_length = 0;

    for (size_t i = 0; i < config.locations.size(); i++)
    {
        const std::string &location_path = config.locations[i].path;
        if (uri.find(location_path) == 0 && location_path.size() > best_match_length)
        {
            best_match = &config.locations[i];
            best_match_length = location_path.size();
        }
    }

    return best_match;
}

void Client::processRequest(const ServerConfig &server_config, int epoll_fd)
{
    if (state == ERROR_413)
    {
        std::cout << "[CLIENT " << fd << "] Gerando resposta 413 (Content-Length excedeu limite)" << std::endl;

        // Criar resposta 413 diretamente
        std::string response_str;
        StatusCodes::http413PayloadTooLarge(response_str, server_config);

        send_buffer = response_str;
        send_offset = 0;
        state = SENDING_RESPONSE;
        keep_alive = false;

        return;
    }

    if (state != PROCESSING)
        return;

    if (!is_chunked && content_length > 0 && headers_end_pos > 0)
    {
        size_t body_size = recv_buffer.size() - headers_end_pos;
        if (body_size >= content_length)
        {
            std::string body = recv_buffer.substr(headers_end_pos, content_length);
            request.setBody(body);
            std::cout << "[CLIENT " << fd << "] Body extraído: " << body.size() << " bytes" << std::endl;
        }
    }

    const LocationConfig *location = findMatchingLocation(server_config, request.getUri());
    if (location && !location->cgi_handlers.empty())
    {
        bool method_allowed = false;
        if (!location->allowed_methods.empty())
        {
            for (size_t i = 0; i < location->allowed_methods.size(); ++i)
            {
                if (location->allowed_methods[i] == request.getMethod())
                {
                    method_allowed = true;
                    break;
                }
            }
        }

        if (!method_allowed)
        {
            std::cout << "[405] Método " << request.getMethod() << " não permitido para: " << request.getUri() << std::endl;
            StatusCodes::http405MethodNotAllowed(this->send_buffer, request, request.getUri(), server_config);
            this->send_offset = 0;
            this->state = SENDING_RESPONSE;
            return;
        }

        std::cout << "[CLIENT " << fd << "] Iniciando CGI para " << request.getUri() << std::endl;
        state = CGI_RUNNING;
        startCgi(request, *location, server_config, epoll_fd);
        return;
    }

    if (response)
        delete response;

    response = new Response(request, server_config);
    send_buffer = response->getResponseHttp();
    send_offset = 0;

    state = SENDING_RESPONSE;

    std::cout << "[CLIENT " << fd << "] Resposta criada: "
              << send_buffer.size() << " bytes" << std::endl;
}

// ========== Envio de resposta ==========

bool Client::sendData()
{
    if (state != SENDING_RESPONSE || !hasDataToSend())
        return false;

    // Enviar chunk do buffer
    size_t remaining = send_buffer.size() - send_offset;
    size_t to_send = remaining; // Pode limitar aqui (ex: 8192 bytes por vez)

    ssize_t sent = write(fd, send_buffer.c_str() + send_offset, to_send);

    if (sent == 0)
    {
        std::cout << "[CLIENT " << fd << "] Conexão fechada pelo cliente durante envio" << std::endl;
        return false;
    }

    if (sent < 0)
    {
        std::cerr << "[CLIENT " << fd << "] Erro ao enviar dados" << std::endl;
        return false;
    }

    send_offset += sent;
    updateLastActivity();

    // Verificar se terminou de enviar
    if (send_offset >= send_buffer.size())
    {
        std::cout << "[CLIENT " << fd << "] Resposta enviada completamente" << std::endl;
        state = DONE;
        return true;
    }

    return false;
}

// ========== Reset para keep-alive ==========

void Client::setState(State s) { state = s; }
void Client::reset()
{
    recv_buffer.clear();
    send_buffer.clear();
    send_offset = 0;

    if (response)
    {
        delete response;
        response = NULL;
    }

    request = HttpRequest();
    content_length = 0;
    headers_end_pos = 0;

    is_chunked = false;
    is_cgi_active = false;
    cgi = CgiState();
    
    state = READING_HEADERS;
    updateLastActivity();

    std::cout << "[CLIENT " << fd << "] Reset para keep-alive (is_cgi_active=false, is_chunked=false)" << std::endl;
}

// ========== Privados ==========

void Client::updateLastActivity()
{
    last_activity = time(NULL);
}

// Cria envp usando EnvBuilder para suportar php-cgi com force-cgi-redirect
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
    std::string script_path = loc.root + req.getPath().substr(loc.path.size());
    std::cout << "[CGI] Script path: " << script_path << std::endl;
    size_t pos = script_path.find('.');
    std::string ext;
    if (pos != std::string::npos )
        ext = script_path.substr(pos);
    else
       ext = "" ;
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

    if (setClosExec(cgi.pipe_in[0]) < 0) {
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

    if (setClosExec(cgi.pipe_in[1]) < 0) {
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
        // CORREÇÃO: Cleanup do pipe_in anterior
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

    if (setClosExec(cgi.pipe_out[0]) < 0) {
        int saved_errno = errno;
        std::cerr << "[CGI] setClosExec(pipe_out[0]) failed: " << strerror(saved_errno) << std::endl;
        // CORREÇÃO: Cleanup de AMBOS os pipes
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

    if (setClosExec(cgi.pipe_out[1]) < 0) {
        int saved_errno = errno;
        std::cerr << "[CGI] setClosExec(pipe_out[1]) failed: " << strerror(saved_errno) << std::endl;
        // CORREÇÃO: Cleanup de AMBOS os pipes
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
    { // filho
        dup2(cgi.pipe_in[0], STDIN_FILENO);
        dup2(cgi.pipe_out[1], STDOUT_FILENO);
        dup2(cgi.pipe_out[1], STDERR_FILENO);
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

    // pai
    freeEnvp(envp);

    // pai
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

    std::cout << "[CLIENT " << fd << "] CGI stdin writable" << std::endl;

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

    // Fazer UMA escrita
    ssize_t w = write(cgi.pipe_in[1], body.c_str() + cgi.body_written,
                      body.size() - cgi.body_written);

    // Salvar errno IMEDIATAMENTE
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

    // w <= 0: erro ou EAGAIN
    if (saved_errno == EAGAIN || saved_errno == EWOULDBLOCK)
    {
        // Sem espaço, epoll chamará novamente
        return;
    }

    std::cerr << "[CLIENT " << fd << "] CGI stdin write error: " << strerror(saved_errno) << std::endl;
    
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, cgi.pipe_in[1], NULL);
    close(cgi.pipe_in[1]);
    cgi.pipe_in[1] = -1;

    if (cgi.pid > 0) {
        std::cerr << "[CLIENT " << fd << "] Killing CGI process (PID=" 
                  << cgi.pid << ") due to write error" << std::endl;
        kill(cgi.pid, SIGKILL);
        int status;
        waitpid(cgi.pid, &status, 0);  // Aguardar a morte do processo
        cgi.pid = -1;
    }

    if (cgi.pipe_out[0] >= 0) {
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, cgi.pipe_out[0], NULL);
        close(cgi.pipe_out[0]);
        cgi.pipe_out[0] = -1;
    }
    if (cgi.pipe_out[1] >= 0) {
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
    return;
}

void Client::handleCgiStdoutReadable(int epoll_fd)
{
    if (!is_cgi_active)
        return;

    std::cout << "[CLIENT " << fd << "] CGI stdout readable" << std::endl;

    char buf[8192];
    // Fazer UM read() apenas
    ssize_t r = read(cgi.pipe_out[0], buf, sizeof(buf));
    
    // Salvar errno IMEDIATAMENTE
    int saved_errno = errno;

    if (r > 0)
    {
        // Sucesso: armazenar dados e retornar
        std::cout << "[CLIENT " << fd << "] CGI read " << r << " bytes" << std::endl;
        cgi.output.append(buf, r);
        return;
    }

    if (r == 0)
    {
        // EOF: stdout fechado
        std::cout << "[CLIENT " << fd << "] CGI stdout EOF" << std::endl;
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, cgi.pipe_out[0], NULL);
        close(cgi.pipe_out[0]);
        cgi.pipe_out[0] = -1;
        // Não finalizar aqui - esperar EPOLLHUP/EPOLLERR que sinalizará término do CGI
        return;
    }

    // r < 0: erro ou EAGAIN
    if (saved_errno == EAGAIN || saved_errno == EWOULDBLOCK)
    {
        // Sem dados disponíveis, epoll chamará novamente
        return;
    }

    // Erro real
    std::cerr << "[CLIENT " << fd << "] CGI read error: " << strerror(saved_errno) << std::endl;
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, cgi.pipe_out[0], NULL);
    close(cgi.pipe_out[0]);
    cgi.pipe_out[0] = -1;
    // Não finalizar aqui - esperar término do CGI via EPOLLHUP
    return;
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
                // Processo morreu por sinal
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

    // ✅ SE CGI FALHOU, RETORNAR 502 IMEDIATAMENTE
    if (cgi_failed)
    {
        std::string error_msg;
        StatusCodes::http502BadGateway(error_msg, request, failure_reason,
                                       config->getServerConfig(server_index));
        send_buffer = error_msg;
        send_offset = 0;
        state = SENDING_RESPONSE;
        return;  // ← Não processar headers CGI
    }

    // ✅ CGI SUCEDEU, processar seu output
    // Separar headers CGI do body
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

    std::string status_line  = "200 OK";
    std::string extra_headers;

    std::istringstream iss(cgi_headers);
    std::string line;
    while (std::getline(iss, line))
    {
        // Remove \r se presente
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);

        if (line.empty())
            continue;

        if (line.find("Status: ") == 0)
        {
            status_line = line.substr(8);
            // Remove \r do status se presente
            if (!status_line.empty() && status_line[status_line.size()-1] == '\r')
                status_line.erase(status_line.size() - 1);
        }
        else
            extra_headers += line + "\r\n";
    }

    StatusCodes::http200CgiResponse(send_buffer, status_line, extra_headers, 
                                    cgi_body, keep_alive);
    send_offset = 0;
    state       = SENDING_RESPONSE;
}