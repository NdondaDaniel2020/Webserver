# include "../include/HttpRequest.hpp"

HttpRequest::HttpRequest()
{
}

HttpRequest::HttpRequest(const ConfigParser& config)
{
    this->config = config;
}

HttpRequest::~HttpRequest()
{
}

HttpRequest::HttpRequest(const HttpRequest& other)
{
    *this = other;
}

HttpRequest& HttpRequest::operator=(const HttpRequest& other)
{
    if (this != &other)
    {
        this->method = other.method;
        this->uri = other.uri;
        this->version = other.version;
        this->headers = other.headers;
        this->body = other.body;
    }
    return *this;
}

HttpRequest HttpRequest::parseHttpRequest(const std::string& raw_request)
{
    std::string line;
    HttpRequest req;
    std::istringstream stream(raw_request);

    while (std::getline(stream, line))
    {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        
        std::istringstream line_stream(line);
        line_stream >> req.method >> req.uri >> req.version;
        
        return (req);
    }
}