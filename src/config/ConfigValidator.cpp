
#ifndef CONFIG_VALIDATOR_HPP
# define CONFIG_VALIDATOR_HPP

# include "ConfigParser.hpp"

namespace ConfigValidator
{
    bool validatePort(int port);
    bool validatePath(const std::string& path);
    bool validateClientMaxBodySize(size_t size);
    bool validateHttpCode(const std::string& code);
    bool validateAutoIndex(const std::string& value);
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
}

#endif