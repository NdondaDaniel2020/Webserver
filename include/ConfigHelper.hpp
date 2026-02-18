
# ifndef CONFIG_HELPER_HPP
# define CONFIG_HELPER_HPP

# include "ConfigParser.hpp"

namespace ConfigHelper
{    
    void parseRoot(const std::string& value, std::string& root);
    void parseAutoIndex(const std::string& value, bool& autoindex);
    void parseRedirect(const std::string& value, int& code, std::string& url);
    void parseClientMaxBodySize(const std::string& value, size_t& max_body_size);
    void parseIndex(const std::string& value, std::vector<std::string>& index_files);
    void extractKeyValue(const std::string& line, std::string& key, std::string& value);
    void parseErrorPage(const std::string& value, std::map<std::string, std::string>& error_pages);
    void parseCommonConfig(const std::string& key, const std::string& value, ServerConfig& server);
    void parseCommonConfig(const std::string& key, const std::string& value, LocationConfig& location);

    namespace Detail {
        void extractTwoValues(const std::string& input, std::string& first, std::string& second);
    }
}

#endif