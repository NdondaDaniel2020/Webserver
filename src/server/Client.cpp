/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nmatondo <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/19 11:33:45 by nmatondo          #+#    #+#             */
/*   Updated: 2026/03/20 14:00:21 by nmatondo         ###   ########.fr       */
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
      cgi(), // default constructor
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
}

Client::Client(const Client &other)
    : fd(other.fd),
      state(other.state),
      last_activity(other.last_activity), // ← agora na posição correta
      recv_buffer(other.recv_buffer),
      send_buffer(other.send_buffer),
      send_offset(other.send_offset),
      request(other.request),
      response(NULL), // cuidado com deep copy abaixo
      keep_alive(other.keep_alive),
      content_length(other.content_length), // ← agora depois de keep_alive
      headers_end_pos(other.headers_end_pos),
      config(other.config),
      cgi(other.cgi),
      is_cgi_active(other.is_cgi_active),
      is_chunked(other.is_chunked),
      server_index(other.server_index)
{
    if (other.response)
        response = new Response(*other.response);
}

Client &Client::operator=(const Client &other)
{
    if (this != &other)
    {
        fd = other.fd;
        state = other.state;
        recv_buffer = other.recv_buffer;
        send_buffer = other.send_buffer;
        send_offset = other.send_offset;
        request = other.request;

        if (response)
            delete response;
        response = other.response ? new Response(*other.response) : NULL;

        keep_alive = other.keep_alive;
        content_length = other.content_length;
        headers_end_pos = other.headers_end_pos;
        last_activity = other.last_activity;
        config = other.config;
        cgi = other.cgi;
        is_cgi_active = other.is_cgi_active;
        is_chunked = other.is_chunked;
        server_index = other.server_index;
    }
    return *this;
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
    state = READING_HEADERS;

    updateLastActivity();

    std::cout << "[CLIENT " << fd << "] Reset para keep-alive" << std::endl;
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
    if (pipe(cgi.pipe_in) < 0 || pipe(cgi.pipe_out) < 0)
    {
        StatusCodes::http502BadGateway(error_msg, req, "Failed to create pipes", server_config);
        send_buffer = error_msg;
        send_offset = 0;
        state = SENDING_RESPONSE;
        return;
    }

    // Seta FD_CLOEXEC em pipe_in[0] e pipe_in[1]
    if (setClosExec(cgi.pipe_in[0]) < 0 || setClosExec(cgi.pipe_in[1]) < 0) {
        std::cerr << "[CGI] Falha ao seta FD_CLOEXEC em pipe_in" << std::endl;
        close(cgi.pipe_in[0]);
        close(cgi.pipe_in[1]);
        close(cgi.pipe_out[0]);
        close(cgi.pipe_out[1]);
        StatusCodes::http502BadGateway(error_msg, req, "Failed to set CLOEXEC on pipes", server_config);
        send_buffer = error_msg;
        send_offset = 0;
        state = SENDING_RESPONSE;
        return;
    }

    // Seta FD_CLOEXEC em pipe_out[0] e pipe_out[1]
    if (setClosExec(cgi.pipe_out[0]) < 0 || setClosExec(cgi.pipe_out[1]) < 0) {
        std::cerr << "[CGI] Falha ao seta FD_CLOEXEC em pipe_out" << std::endl;
        close(cgi.pipe_in[0]);
        close(cgi.pipe_in[1]);
        close(cgi.pipe_out[0]);
        close(cgi.pipe_out[1]);
        StatusCodes::http502BadGateway(error_msg, req, "Failed to set CLOEXEC on pipes", server_config);
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
        freeEnvp(envp);
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

    // Adiciona pipes ao epoll (level-triggered; evita perda de eventos com EPOLLET)
    epoll_event ev;
    ev.events = EPOLLOUT | EPOLLHUP;
    ev.data.fd = cgi.pipe_in[1];
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, cgi.pipe_in[1], &ev);

    ev.events = EPOLLIN | EPOLLHUP;
    ev.data.fd = cgi.pipe_out[0];
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, cgi.pipe_out[0], &ev);
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

    ssize_t w = write(cgi.pipe_in[1], body.c_str() + cgi.body_written,
                      body.size() - cgi.body_written);

    if (w > 0)
    {
        cgi.body_written += static_cast<size_t>(w);
        if (cgi.body_written >= body.size())
        {
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, cgi.pipe_in[1], NULL);
            close(cgi.pipe_in[1]);
            cgi.pipe_in[1] = -1;
        }
    }
    else if (w == 0)
        std::cout << "[CLIENT " << fd << "] CGI stdin write retornou 0" << std::endl;
    else  // w < 0
    {
        // Erro em write: pipe quebrado, processo morreu, etc
        std::cerr << "[CLIENT " << fd << "] CGI stdin write error: " << strerror(errno) << std::endl;
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, cgi.pipe_in[1], NULL);
        close(cgi.pipe_in[1]);
        cgi.pipe_in[1] = -1;
    }
}

void Client::handleCgiStdoutReadable(int epoll_fd)
{
    if (!is_cgi_active)
        return;

    std::cout << "[CLIENT " << fd << "] CGI stdout readable" << std::endl;

    char buf[8192];
    ssize_t r;
    while ((r = read(cgi.pipe_out[0], buf, sizeof(buf))) > 0)
    {
        std::cout << "[CLIENT " << fd << "] CGI read " << r << " bytes" << std::endl;
        cgi.output.append(buf, r);
    }
    
    if (r < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
    {
        std::cerr << "[CLIENT " << fd << "] CGI read error: " << strerror(errno) << std::endl;
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, cgi.pipe_out[0], NULL);
        close(cgi.pipe_out[0]);
        cgi.pipe_out[0] = -1;
        finishCgiAndGenerateResponse();
        return;
    }

    if (r == 0)
    {
        std::cout << "[CLIENT " << fd << "] CGI stdout EOF" << std::endl;
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, cgi.pipe_out[0], NULL);
        close(cgi.pipe_out[0]);
        cgi.pipe_out[0] = -1;
        finishCgiAndGenerateResponse();
        return;
    }

    if (errno == EAGAIN || errno == EWOULDBLOCK)
    {
        int status;
        pid_t result = waitpid(cgi.pid, &status, WNOHANG);
        if (result == cgi.pid)
        {
            std::cout << "[CLIENT " << fd << "] CGI processo terminou, drenando pipe..." << std::endl;
            while ((r = read(cgi.pipe_out[0], buf, sizeof(buf))) > 0)
                cgi.output.append(buf, r);

            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, cgi.pipe_out[0], NULL);
            close(cgi.pipe_out[0]);
            cgi.pipe_out[0] = -1;
            cgi.pid = -1;
            finishCgiAndGenerateResponse();
        }
    }
    else
    {
        std::cerr << "[CLIENT " << fd << "] CGI read error: " << strerror(errno) << std::endl;
    }
}

void Client::finishCgiAndGenerateResponse()
{
    int status;
    if (cgi.pid > 0)
    {
        waitpid(cgi.pid, &status, WNOHANG);
        cgi.pid = -1;
    }

    is_cgi_active = false;

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

    std::ostringstream oss;
    oss << "HTTP/1.1 " << status_line << "\r\n"
        << extra_headers
        << "Content-Length: " << cgi_body.size() << "\r\n"
        << "Connection: " << (keep_alive ? "keep-alive" : "close") << "\r\n"
        << "\r\n"
        << cgi_body;

    send_buffer = oss.str();
    send_offset = 0;
    state       = SENDING_RESPONSE;
}