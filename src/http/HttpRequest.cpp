

#include "HttpRequest.hpp"
#include "StringUtils.hpp"
#include <iostream>

HttpRequest::HttpRequest() : method(""), uri(""), version(""), body("")
{
}

HttpRequest::~HttpRequest()
{
}

HttpRequest::HttpRequest(const HttpRequest &other)
    : query(other.query),
      path(other.path),
      method(other.method),
      uri(other.uri),
      version(other.version),
      headers(other.headers),
      body(other.body)
{
}

HttpRequest &HttpRequest::operator=(const HttpRequest &other)
{
    if (this != &other)
    {
        this->method = other.method;
        this->uri = other.uri;
        this->path = other.path;
        this->query = other.query;
        this->version = other.version;
        this->headers = other.headers;
        this->body = other.body;
    }
    return *this;
}

HttpRequest HttpRequest::parse(const std::string &raw_request)
{
    HttpRequest req;
    std::istringstream stream(raw_request);
    std::string line;

    // Parse request line
    if (std::getline(stream, line))
    {
        // Remove \r if present
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        req.parseRequestLine(line);
    }

    // Parse headers
    req.parseHeaders(stream);

    // Parse body
    req.parseBody(stream);

    return req;
}

void HttpRequest::parseRequestLine(const std::string &line)
{
    std::istringstream iss(line);
    iss >> this->method >> this->uri >> this->version;

    size_t pos = this->uri.find('?');
    if (pos != std::string::npos)
    {
        this->path = this->uri.substr(0, pos);
        this->query = this->uri.substr(pos + 1);
    }
    else
    {
        this->path = this->uri;
        this->query = "";
    }

    std::cout << "[REQUEST] " << this->method << " " << this->uri << " " << this->version << std::endl;
    
}

void HttpRequest::parseHeaders(std::istringstream &stream)
{
    std::string line;

    while (std::getline(stream, line))
    {
        // Remove \r if present
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);

        // Empty line indicates end of headers
        if (line.empty())
            break;

        // Parse header: "Key: Value"
        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos)
        {
            std::string key = line.substr(0, colon_pos);
            std::string value = line.substr(colon_pos + 1);

            // Trim whitespace
            key = StringUtils::trim(key);
            value = StringUtils::trim(value);

            this->headers[key] = value;
        }
    }
}

void HttpRequest::parseBody(std::istringstream &stream)
{
    std::string line;
    std::ostringstream body_stream;

    while (std::getline(stream, line))
    {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        body_stream << line << "\n";
    }

    this->body = body_stream.str();
    if (!this->body.empty() && this->body[this->body.size() - 1] == '\n')
        this->body.erase(this->body.size() - 1);
}

// Getters
const std::string &HttpRequest::getMethod() const
{
    return this->method;
}

const std::string &HttpRequest::getQuery() const
{
    return this->query;
}

const std::string &HttpRequest::getPath() const
{
    return this->path;
}

const std::string &HttpRequest::getUri() const
{
    return this->uri;
}

const std::string &HttpRequest::getVersion() const
{
    return this->version;
}

const std::map<std::string, std::string> &HttpRequest::getHeaders() const
{
    return this->headers;
}

const std::string &HttpRequest::getBody() const
{
    return this->body;
}

// Setters
void HttpRequest::setBody(const std::string &body)
{
    this->body = body;
}

// Header utilities
bool HttpRequest::hasHeader(const std::string &key) const
{
    return this->headers.find(key) != this->headers.end();
}

std::string HttpRequest::getHeader(const std::string &key) const
{
    std::map<std::string, std::string>::const_iterator it = this->headers.find(key);
    if (it != this->headers.end())
        return it->second;
    return "";
}

size_t HttpRequest::getContentLength() const
{
    std::string content_length = getHeader("Content-Length");
    if (content_length.empty())
        return 0;
    return atoi(content_length.c_str());
}