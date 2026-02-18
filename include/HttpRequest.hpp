
#ifndef HTTPREQUEST_HPP
# define HTTPREQUEST_HPP

# include "ConfigParser.hpp"



class HttpRequest
{
    private:
        std::string method;
        std::string uri;
        std::string version;
        std::map<std::string, std::string> headers;
        std::string body;
        ConfigParser config;
    
    public:
        HttpRequest();
        ~HttpRequest();
        HttpRequest(const HttpRequest& other);
        HttpRequest(const ConfigParser& config);
        HttpRequest& operator=(const HttpRequest& other);

        const std::vector<HttpRequest>& getRequests() const;
       static HttpRequest parseHttpRequest(const std::string& raw_request);
};

#endif