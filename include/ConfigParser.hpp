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
# include "StringUtils.hpp"

struct LocationConfig {
    std::string path;
    std::vector<std::string> allowed_methods; 
    std::string root;
    bool autoindex;
    std::vector<std::string> index_files;
    std::string cgi_path;
    int redirect_code;
    std::string redirect_url;
    std::vector<std::string> cgi_extensions;
    size_t client_max_body_size;
    std::string upload_dir;

    LocationConfig() : autoindex(false), 
                       redirect_code(0), 
                       client_max_body_size(0) {}
};

struct ServerConfig {
    int port;
    std::string server_name;
    std::string root;
    std::map<std::string, std::string> error_pages;
    std::vector<std::string> index_files;
    size_t client_max_body_size;
    std::vector<LocationConfig> locations;
    
    ServerConfig() : port(8080), 
                     server_name("localhost"),
                     root("www"),
                     client_max_body_size(0) {}
};

class ConfigParser {

    private:
        std::vector<ServerConfig> servers;
        void parseServerBlock(std::ifstream& file, ServerConfig& server);
        void parseLocationBlock(std::ifstream& file, LocationConfig& location);

        // Generic parsing helpers
        void parseRoot(const std::string& value, std::string& root);
        void parseIndex(const std::string& value, std::vector<std::string>& index_files);
        void parseClientMaxBodySize(const std::string& value, size_t& max_body_size);
        void parseErrorPage(const std::string& value, std::map<std::string, std::string>& error_pages);
        void parseAutoIndex(const std::string& value, bool& autoindex);
        void parseRedirect(const std::string& value, int& code, std::string& url); // Example for location return


    public:
        ConfigParser();
        ~ConfigParser();

        bool loadFromFile(const std::string& filename);
        ServerConfig getServerConfig(size_t index) const;
        size_t getServerCount() const;
};


#endif