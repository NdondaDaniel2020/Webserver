
# include "../../include/ConfigParser.hpp"

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

bool ConfigParser::loadFromFile(const std::string& filename) 
{
    std::ifstream file(filename.c_str());

    if (!file.is_open())
    {
        throw std::runtime_error("Error: Could not open config file: " + filename);
        return (false);
    }
    
    std::string line;
    while (std::getline(file, line))
    {
        line = StringUtils::trim(line);

        if (line.empty() || line[0] == '#')
            continue;
        
        if (line.find("server") != std::string::npos && line.find("{") != std::string::npos)
        {
            ServerConfig config;
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
            break;

        if (!line.empty() && line[line.size() - 1] == ';')
            line.erase(line.size() - 1);
        
        std::istringstream iss(line);
        std::string key, value;
        iss >> key;
        std::getline(iss, value);

        if (key == "listen")
        {
            server.port = atoi(StringUtils::trim(value).c_str());
        }
        else if (key == "server_name")
        {
            server.server_name = StringUtils::trim(value);
        }
        else if (key == "root")
        {
            parseRoot(value, server.root);
        }
        else if (key == "error_page")
        {
            parseErrorPage(value, server.error_pages);
        }
        else if (key == "index")
        {
            parseIndex(value, server.index_files);
        }
        else if (key == "client_max_body_size")
        {
            parseClientMaxBodySize(value, server.client_max_body_size);
        }
        else if (key == "location")
        {
            LocationConfig location;
            std::string path;
            std::istringstream lss(StringUtils::trim(value));
            lss >> path;
            
            // Remove opening brace if present in the location line
            size_t bracePos = path.find('{');
            if (bracePos != std::string::npos)
                path = path.substr(0, bracePos);
            
            location.path = path;
            parseLocationBlock(file, location);
            server.locations.push_back(location);
        }
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
        
        std::istringstream iss(line);
        std::string key, value;
        iss >> key;
        std::getline(iss, value);

        if (key == "root")
            parseRoot(value, location.root);
        else if (key == "index")
            parseIndex(value, location.index_files);
        else if (key == "client_max_body_size")
            parseClientMaxBodySize(value, location.client_max_body_size);
        // Add more location specific directives here if needed
        else if (key == "autoindex")
            parseAutoIndex(value, location.autoindex);
        else if (key == "return")
            parseRedirect(value, location.redirect_code, location.redirect_url);
    }
}

// Generic Helpers

void ConfigParser::parseRoot(const std::string& value, std::string& root)
{
    root = StringUtils::trim(value);
}

void ConfigParser::parseIndex(const std::string& value, std::vector<std::string>& index_files)
{
    std::istringstream ivs(StringUtils::trim(value));
    std::string index_file;
    while (ivs >> index_file)
    {
        index_files.push_back(index_file);
    }
}

void ConfigParser::parseClientMaxBodySize(const std::string& value, size_t& max_body_size)
{
    max_body_size = StringUtils::parseSize(value);
}

void ConfigParser::parseErrorPage(const std::string& value, std::map<std::string, std::string>& error_pages)
{
    std::istringstream evs(StringUtils::trim(value));
    std::string code, path;
    evs >> code >> path;
    error_pages[code] = path;
}

void ConfigParser::parseAutoIndex(const std::string& value, bool& autoindex)
{
    std::string val = StringUtils::trim(value);
    if (val == "on")
        autoindex = true;
    else
        autoindex = false;
}

void ConfigParser::parseRedirect(const std::string& value, int& code, std::string& url)
{
    std::istringstream iss(StringUtils::trim(value));
    iss >> code >> url;
}

ServerConfig ConfigParser::getServerConfig(size_t index) const
{
    if (index < servers.size())
        return servers[index];
    return ServerConfig();
}