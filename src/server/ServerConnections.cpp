# include "Server.hpp"

int Server::createServerSocket(const std::string &interface, int port)
{
    int server_fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (server_fd < 0)
    {
        perror("socket");
        return -1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(ipToHex(interface));
    addr.sin_port = htons(port);

    if (bind(server_fd, (sockaddr *)&addr, sizeof(addr)) < 0)
    {
        std::cerr << "[ERRO] Porta " << port << " já está ocupada" << std::endl;
        close(server_fd);
        return -1;
    }

    if (listen(server_fd, SOMAXCONN) < 0)
    {
        perror("listen");
        close(server_fd);
        return -1;
    }

    std::cout << "Servidor ouvindo na porta " << port
              << " http://" << interface << ":" << port << std::endl;
    return server_fd;
}

void Server::newConnection(int fd)
{
    int client_fd = accept(fd, NULL, NULL);
    if (client_fd < 0)
    {
        perror("accept");
        return;
    }

    if (setClosExec(client_fd) < 0) {
        std::cerr << "[ERRO] Falha ao seta FD_CLOEXEC no client_fd" << std::endl;
        close(client_fd);
        return;
    }

    fcntl(client_fd, F_SETFL, O_NONBLOCK);

    std::cout << "[+] Cliente conectado fd=" << client_fd << std::endl;
    
    int server_index = -1;
    for (int i = 0; i < this->port_count; i++)
    {
        if (fd == this->servers[i])
        {
            server_index = i;
            break;
        }
    }
    
    Client *client = NULL;
    try
    {
        client = new Client(client_fd, &this->config, server_index);
    }
    catch(const std::exception& e)
    {
        std::cerr << "[ERROR] Failed to create Client: " << e.what() << std::endl;
        close(client_fd);
        return;
    }

    epoll_event cev;
    cev.events = EPOLLIN | EPOLLOUT;
    cev.data.fd = client_fd;

    if (epoll_ctl(this->epoll_fd, EPOLL_CTL_ADD, client_fd, &cev) < 0)
    {
        perror("epoll_ctl");
        delete client;
        close(client_fd);
        std::cerr << "[ERRO] Falha ao adicionar cliente à lista epoll" << std::endl;
        return;
    }
    this->clients[client_fd] = client;
}

void Server::closeClient(int fd)
{
    std::map<int, Client *>::iterator it = this->clients.find(fd);
    if (it == this->clients.end())
        return;

    Client *client = it->second;

    int cgi_out = -1;
    int cgi_in = -1;
    int cgi_err = -1;
    if (client->isCgiActive())
    {
        cgi_out = client->getCgiOutFd();
        cgi_in = client->getCgiInFd();
        cgi_err = client->getCgiErrFd();
    }

    client->cleanupCgiIfActive(this->epoll_fd);
    epoll_ctl(this->epoll_fd, EPOLL_CTL_DEL, fd, NULL);

    if (cgi_out >= 0)
        cgi_fd_map.erase(cgi_out);
    if (cgi_in >= 0)
        cgi_fd_map.erase(cgi_in);
    if (cgi_err >= 0)
        cgi_fd_map.erase(cgi_err);

    this->clients.erase(it);
    close(fd);
    delete client;

    std::cout << "[-] Cliente fd=" << fd << " fechado" << std::endl;
}

void Server::checkTimeout()
{
    time_t now = time(NULL);

    for (std::map<int, Client *>::iterator it = clients.begin();
         it != clients.end(); ++it)
    {
        int client_fd = it->first;
        Client *client = it->second;

        if (client->isCgiActive())
        {
            time_t cgi_timeout = client->getCgiTimeout();
            if (cgi_timeout > 0 && now - client->getCgiStartTime() > cgi_timeout)
            {
                std::cout << "[TIMEOUT] CGI fd=" << client_fd
                          << " excedeu " << cgi_timeout << "s" << std::endl;
                client->cleanupCgiIfActive(this->epoll_fd);
                client->sendTimeoutResponse();
            }
        }

        time_t client_timeout = client->getTimeout();
        if (client_timeout == 0)
            client_timeout = TIMEOUT_SECONDS;
        if (client_timeout > 0 && now - client->getLastActivity() > client_timeout)
        {
            std::cout << "[TIMEOUT] Cliente fd=" << client_fd
                      << " inactivo por "
                      << (now - client->getLastActivity())
                      << "s (limite: " << client_timeout << "s)" << std::endl;
            client->sendTimeoutResponse();
        }
    }
}
