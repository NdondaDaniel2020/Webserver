
# include "../../include/ConfigParser.hpp"
# include "../../include/ConfigHelper.hpp"

ConfigParser::ConfigParser() 
{
}

ConfigParser::~ConfigParser() 
{
}

size_t ConfigParser::getServerCount() const
{
    return servers.size();
}

ServerConfig ConfigParser::getServerConfig(size_t index) const
{
    if (index < servers.size())
        return servers[index];
    return ServerConfig();
}

bool ConfigParser::loadFromFile(const std::string& filename) 
{
    std::string line;
    std::ifstream file;
    ServerConfig config;

    openFile(file, filename, ERROR_OPENING_FILE);
    while (std::getline(file, line))
    {
        line = StringUtils::trim(line);

        if (line.empty() || line[0] == '#')
            continue;
        
        if (line.find("server") != std::string::npos && line.find("{") != std::string::npos)
        {
            parseServerBlock(file, config);
            servers.push_back(config);
        }
    }
    file.close();
    if (servers.empty())
    {
        throw std::runtime_error("Error: No server configurations found in file: " + filename);
        return (false);
    }
    return (true);
}

void ConfigParser::parseServerBlock(std::ifstream& file, ServerConfig& server) 
{
    std::string line;
    
    while (std::getline(file, line))
    {
        line = StringUtils::trim(line);

        if (line.empty() || line[0] == '#')
            continue;

        if (line == "}")
            break ;

        if (!line.empty() && line[line.size() - 1] == ';')
            line.erase(line.size() - 1);
        if (line[line.size() - 1] != ';' && line.find("location") == std::string::npos)
            throw std::runtime_error("Missing semicolon: " + line);
        
        std::string key, value;
        ConfigHelper::extractKeyValue(line, key, value);

        if (key == "listen")
            server.port = atoi(StringUtils::trim(value).c_str());
        else if (key == "server_name")
            server.server_name = StringUtils::trim(value);
        else if (key == "error_page")
            ConfigHelper::parseErrorPage(value, server.error_pages);
        else if (key == "location")
        {
            LocationConfig location;
            std::string path;
            std::istringstream lss(StringUtils::trim(value));
            lss >> path;
            
            size_t bracePos = path.find('{');
            if (bracePos != std::string::npos)
                path = path.substr(0, bracePos);
            
            location.path = path;
            parseLocationBlock(file, location);
            server.locations.push_back(location);
        }
        else
            ConfigHelper::parseCommonConfig(key, value, server);
    }
}

void ConfigParser::parseLocationBlock(std::ifstream& file, LocationConfig& location)
{
    std::string line;
    
    while (std::getline(file, line))
    {
        line = StringUtils::trim(line);

        if (line.empty() || line[0] == '#')
            continue;
        
        if (line == "}")
            break;

        if (!line.empty() && line[line.size() - 1] == ';')
            line.erase(line.size() - 1);
        
        std::string key, value;
        ConfigHelper::extractKeyValue(line, key, value);

        if (key == "autoindex")
            ConfigHelper::parseAutoIndex(value, location.autoindex);
        else if (key == "return")
            ConfigHelper::parseRedirect(value, location.redirect_code, location.redirect_url);
        else
            ConfigHelper::parseCommonConfig(key, value, location);
    }
}

