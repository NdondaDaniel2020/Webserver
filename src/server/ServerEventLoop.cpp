# include "Server.hpp"

void Server::handleClientData(int fd)
{
    std::map<int, Client *>::iterator it = this->clients.find(fd);
    if (it == this->clients.end())
    {
        std::cerr << "[ERRO] Cliente fd=" << fd << " não encontrado" << std::endl;
        return;
    }

    Client *client = it->second;

    if (client->getState() == Client::READING_HEADERS ||
        client->getState() == Client::READING_BODY)
    {
        char buf[40960]; // 8192 
        int r = read(fd, buf, sizeof(buf));

        if (r <= 0)
        {
            std::cout << "[-] Cliente desconectado fd=" << fd << std::endl;
            closeClient(fd);
            return;
        }

        client->appendRecvData(buf, r);

        if (client->IsHeaderRequestComplete() && !client->isHeaderValidated())
            client->processHeaderRequest(this->config.getServerConfig(client->getServerIndex()));
        if (client->isRequestComplete())
        {
            client->processRequest(this->config.getServerConfig(client->getServerIndex()), this->epoll_fd);
            if (client->isCgiActive())
            {
                int out_fd = client->getCgiOutFd();
                int in_fd = client->getCgiInFd();
                if (out_fd >= 0)
                    cgi_fd_map[out_fd] = client;
                if (in_fd >= 0)
                    cgi_fd_map[in_fd] = client;
            }
        }
    }
    else if (client->getState() == Client::SENDING_RESPONSE)
    {
        if (client->hasDataToSend())
        {
            bool finished = client->sendData();
            if (finished)
            {
                if (client->isKeepAlive())
                    client->reset();
                else
                    closeClient(fd);
            }
        }
    }
}

bool Server::isServerSocket(int fd) const
{
    for (int i = 0; i < this->port_count; i++)
    {
        if (fd == this->servers[i])
            return true;
    }
    return false;
}