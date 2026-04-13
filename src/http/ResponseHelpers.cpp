# include "ResponseHelpers.hpp"
# include "FileUtils.hpp"

const LocationConfig *findMatchingLocation(const ServerConfig &config, const std::string &uri)
{
    std::string ext = "";
    size_t last_dot = uri.find_last_of('.');
    if (last_dot != std::string::npos && last_dot < uri.length() - 1)
        ext = uri.substr(last_dot);
    if (!ext.empty())
    {
        for (size_t i = 0; i < config.locations.size(); i++)
        {
            const LocationConfig &loc = config.locations[i];
            std::map<std::string, std::string>::const_iterator it = loc.cgi_handlers.find(ext);
            if (it != loc.cgi_handlers.end())
                return &loc;
        }
    }
    size_t best_match_length = 0;
    const LocationConfig *best_match = NULL;
    for (size_t i = 0; i < config.locations.size(); i++)
    {
        const std::string &location_path = config.locations[i].path;
        if (uri.find(location_path) == 0 && location_path.size() > best_match_length)
        {
            best_match = &config.locations[i];
            best_match_length = location_path.size();
            std::cout << "New best match found: " << best_match->path << std::endl;
        }
    }

    if (!best_match)
        return NULL;
    std::string request_path = uri;
    if (best_match->path.size() <= request_path.size() && 
        request_path.substr(0, best_match->path.size()) == best_match->path)
        request_path = request_path.substr(best_match->path.size());    
    if (request_path.empty())
        request_path = "/";
    size_t last_slash = request_path.find_last_of('/');
    size_t last_dot_in_path = request_path.find_last_of('.');
    if (last_dot_in_path != std::string::npos && last_dot_in_path > last_slash)
        request_path = request_path.substr(0, last_slash + 1);
    std::string full_path = best_match->root + request_path;
    if (!isDirectory(full_path))
        return NULL;
    return best_match;
}

bool validateAllowedMethod(const ServerConfig &config, const HttpRequest &request)
{
    const LocationConfig *location = findMatchingLocation(config, request.getUri());

    if (!location)
        return false;

    if (location && !location->allowed_methods.empty())
    {
        std::vector<std::string>::const_iterator it = std::find(
            location->allowed_methods.begin(),
            location->allowed_methods.end(),
            request.getMethod());

        if (it == location->allowed_methods.end())
            return false;
    }
    return true;
}

std::string removeLocationInUri(const std::string &uri, const LocationConfig *location)
{
    std::string uri_without_location = uri;
    if (location && !location->path.empty())
    {
        if (uri.find(location->path) == 0)
        {
            uri_without_location = uri.substr(location->path.length());
            if (uri_without_location.empty() || uri_without_location[0] != '/')
                uri_without_location = "/" + uri_without_location;
        }
    }
    return uri_without_location;
}

std::string getUploadDir(const ServerConfig &config, const HttpRequest &request)
{
    const LocationConfig *location = findMatchingLocation(config, request.getUri());

    if (location && location->client_max_body_size > 0 &&
        request.getBody().size() > location->client_max_body_size)
    {
        std::cout << "[413] Body size (" << request.getBody().size()
                  << ") excede limite da location ("
                  << location->client_max_body_size << ")" << std::endl;
        return "";
    }

    std::string upload_dir = config.root;

    if (location && !location->upload_dir.empty())
    {
        if (location->upload_dir[0] != '/')
            upload_dir = config.root + "/" + location->upload_dir;
        else
            upload_dir = location->upload_dir;
    }

    return upload_dir;
}

void handleRedirect(std::string &response_str, const LocationConfig *location, const HttpRequest &request, const ServerConfig &config)
{
    if (location->redirect_url.empty())
    {
        if (location->redirect_code == 400)
            return StatusCodes::http400BadRequest(response_str, "Bad Request", config, request);
        else if (location->redirect_code == 401)
            return StatusCodes::http401Unauthorized(response_str, request, "Unauthorized", config);
        else if (location->redirect_code == 403)
            return StatusCodes::http403Forbidden(response_str, request, "", config);
        else if (location->redirect_code == 404)
            return StatusCodes::http404NotFound(response_str, request, "", config);
        else if (location->redirect_code == 405)
            return StatusCodes::http405MethodNotAllowed(response_str, request, "", config);
        else if (location->redirect_code == 408)
            return StatusCodes::http408RequestTimeout(response_str, config, request);
        else if (location->redirect_code == 409)
            return StatusCodes::http409Conflict(response_str, request, "Conflict", config);
        else if (location->redirect_code == 411)
            return StatusCodes::http411LengthRequired(response_str, config, request);
        else if (location->redirect_code == 413)
            return StatusCodes::http413PayloadTooLarge(response_str, config, request);
        else if (location->redirect_code == 414)
            return StatusCodes::http414UriTooLong(response_str, config, request);
        else if (location->redirect_code == 415)
            return StatusCodes::http415UnsupportedMediaType(response_str, request, config);
        else if (location->redirect_code == 429)
            return StatusCodes::http429TooManyRequests(response_str, request, config);
        else if (location->redirect_code == 500)
            return StatusCodes::http500InternalServerError(response_str, request, "Internal Server Error", config);
        else if (location->redirect_code == 501)
            return StatusCodes::http501NotImplemented(response_str, request, "Not Implemented", config);
        else if (location->redirect_code == 502)
            return StatusCodes::http502BadGateway(response_str, request, "Bad Gateway", config);
        else if (location->redirect_code == 503)
            return StatusCodes::http503ServiceUnavailable(response_str, request, config);
        else if (location->redirect_code == 504)
            return StatusCodes::http504GatewayTimeout(response_str, request, config);
        else if (location->redirect_code == 505)
            return StatusCodes::http505VersionNotSupported(response_str, config, request);        
    }
    else
    {
        std::ostringstream oss;
        oss << "HTTP/1.1 " << location->redirect_code;

        if (location->redirect_code == 300)
            oss << " Multiple Choices\r\n";
        else if (location->redirect_code == 301)
            oss << " Moved Permanently\r\n";
        else if (location->redirect_code == 302)
            oss << " Found\r\n";
        else if (location->redirect_code == 303)
            oss << " See Other\r\n";
        else if (location->redirect_code == 307)
            oss << " Temporary Redirect\r\n";
        else if (location->redirect_code == 308)
            oss << " Permanent Redirect\r\n";
        else
            oss << " Redirect\r\n";

        oss << "Location: " << location->redirect_url << "\r\n";
        oss << "Content-Length: 0\r\n";
        oss << "Connection: close\r\n\r\n";
        response_str = oss.str();
    }
}
