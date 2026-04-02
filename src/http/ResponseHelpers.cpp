# include "ResponseHelpers.hpp"

// Find the most specific matching location for a given URI
const LocationConfig *findMatchingLocation(const ServerConfig &config, const std::string &uri)
{
    const LocationConfig *best_match = NULL;
    size_t best_match_length = 0;

    for (size_t i = 0; i < config.locations.size(); i++)
    {
        const std::string &location_path = config.locations[i].path;

        if (uri.find(location_path) == 0)
        {
            if (location_path.size() > best_match_length)
            {
                best_match = &config.locations[i];
                best_match_length = location_path.size();
            }
        }
    }

    return best_match;
}

// Validate if the HTTP method is allowed for the request
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

// Remove the location prefix from a URI
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

// Check if a file is in the protected files list
bool isProtectedFile(const std::vector<std::string> &protected_files, const std::string &filename)
{
    for (size_t i = 0; i < protected_files.size(); ++i)
    {
        if (filename == protected_files[i])
            return true;
    }
    return false;
}

// Get the upload directory for the current request
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

// Handle HTTP redirect responses
void handleRedirect(std::string &response_str, int code, const std::string &url)
{
    std::ostringstream oss;
    oss << "HTTP/1.1 " << code;

    if (code == 301)
        oss << " Moved Permanently\r\n";
    else if (code == 302)
        oss << " Found\r\n";
    else if (code == 307)
        oss << " Temporary Redirect\r\n";
    else if (code == 308)
        oss << " Permanent Redirect\r\n";

    oss << "Location: " << url << "\r\n";
    oss << "Content-Length: 0\r\n";
    oss << "Connection: close\r\n\r\n";
    response_str = oss.str();
}
