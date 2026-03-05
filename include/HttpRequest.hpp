
#ifndef HTTPREQUEST_HPP
# define HTTPREQUEST_HPP

# include <string>
# include <map>
# include <vector>
# include <sstream>

class HttpRequest
{
    private:
        std::string method;
        std::string uri;
        std::string version;
        std::map<std::string, std::string> headers;
        std::string body;

    public:
        HttpRequest();
        ~HttpRequest();
        HttpRequest(const HttpRequest& other);
        HttpRequest& operator=(const HttpRequest& other);

        // Parse raw HTTP request
        static HttpRequest parse(const std::string& raw_request);

        // Getters
        const std::string& getQuery() const;
        const std::string& getPath() const;
        const std::string& getMethod() const;
        const std::string& getUri() const;
        const std::string& getVersion() const;
        const std::map<std::string, std::string>& getHeaders() const;
        const std::string& getBody() const;
        
        // Setters
        void setBody(const std::string& body);
        
        // Header utilities
        bool hasHeader(const std::string& key) const;
        std::string getHeader(const std::string& key) const;
        size_t getContentLength() const;

    private:
        void parseRequestLine(const std::string& line);
        void parseHeaders(std::istringstream& stream);
        void parseBody(std::istringstream& stream);
};

#endif