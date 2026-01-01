
#ifndef CONFIG_PARSER_HPP
# define CONFIG_PARSER_HPP

# include <string>
# include <map>
# include <vector>
# include <fstream>
# include <sstream>
# include <stdexcept>
# include <algorithm>
# include <iostream>

struct ServerConfig {
    int port;
    std::string server_name;
    std::string root;
    std::map<std::string, std::string> error_pages;
    std::vector<std::string> index_files;
    size_t client_max_body_size;
    
    ServerConfig() : port(8080), 
                     server_name("localhost"),
                     root("www"),
                     client_max_body_size(10485760) {} // 10MB padrão
};

class ConfigParser {

    private:
        std::vector<ServerConfig> servers;
        std::string trim(const std::string& str);
        void parseServerBlock(std::ifstream& file, ServerConfig& server);

    public:
        ConfigParser();
        ~ConfigParser();

        bool loadFromFile(const std::string& filename);
        ServerConfig getServerConfig(size_t index) const;
        size_t getServerCount() const;
};


#endif