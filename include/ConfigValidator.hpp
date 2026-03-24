
#ifndef CONFIG_VALIDATOR_HPP
# define CONFIG_VALIDATOR_HPP

# include <sys/stat.h>
# include "ConfigParser.hpp"

#include <netdb.h>      // getaddrinfo, freeaddrinfo, gai_strerror
#include <arpa/inet.h>  // inet_ntop
#include <sys/socket.h> // sockaddr
#include <netinet/in.h> // sockaddr_in, sockaddr_in6

namespace ConfigValidator
{
    bool validatePort(std::string port);
    bool validatePath(const std::string& path);
    bool validateClientMaxBodySize(size_t size);
    bool validateHttpCode(const std::string& code);
    bool validateAutoIndex(std::string value);
    bool validateConfigFile(const std::string& filename);
    bool validateServerConfig(const ServerConfig& server);
    bool validateRedirect(const LocationConfig& location);
    bool validateCgiConfig(const LocationConfig& location);
    bool validateUploadDir(const LocationConfig& location);
    bool validateLocationConfig(const LocationConfig& location);
    bool validateAllowedMethods(const LocationConfig& location);
    bool validateConfig(const std::vector<ServerConfig>& servers);
    bool validateIndexFiles(const std::vector<std::string>& index_files);
    bool validateErrorPages(const std::map<std::string, std::string>& error_pages);
    bool validateServerName(std::string& server_name);
    bool validateReturn(std::string value);
    void validateServerConfigDefault(const ServerConfig& server);
    void validateLocationConfigDefault(const LocationConfig& location);
}

#endif