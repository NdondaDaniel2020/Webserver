# ifndef HTTPMETHODS_HPP
# define HTTPMETHODS_HPP

# include "HttpRequest.hpp"

namespace HttpMethods
{
    void handleGet(HttpRequest& request);
    void handlePost(HttpRequest& request);
    void handleDelete(HttpRequest& request);

    bool isSupportedMethod(const std::string& method);
    bool isMethodWithBody(const std::string& method);
}   

#endif