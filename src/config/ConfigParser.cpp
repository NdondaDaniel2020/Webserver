
# include "../../include/ConfigParser.hpp"

ConfigParser::ConfigParser() 
{
}

ConfigParser::~ConfigParser() 
{
}

std::string ConfigParser::trim(const std::string& str) 
{
    size_t first = str.find_first_not_of(" \t\r\n");
    size_t last = str.find_last_not_of(" \t\r\n");
    if (first == std::string::npos || last == std::string::npos)
        return "";
    return str.substr(first, last - first + 1);
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
        line = trim(line);

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
        line = trim(line);

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
        
        if (key == "port")
        {
            server.port = atoi(trim(value).c_str());
        }
        else if (key == "server_name")
        {
            server.server_name = trim(value);
        }
        else if (key == "root")
        {
            server.root = trim(value);
        }
        else if (key == "error_page")
        {
            std::istringstream evs(trim(value));
            std::string code, path;
            evs >> code >> path;
            server.error_pages[code] = path;
        }
        else if (key == "index")
        {
            std::istringstream ivs(trim(value));
            std::string index_file;
            while (ivs >> index_file)
            {
                server.index_files.push_back(index_file);
            }
        }
        else if (key == "client_max_body_size")
        {
            std::string size_str = trim(value);
            // Converter 10M para bytes
            size_t multiplier = 1;
            if (!size_str.empty() && (size_str[size_str.size() - 1] == 'M' || size_str[size_str.size() - 1] == 'm'))
            {
                multiplier = 1024 * 1024;
                size_str.erase(size_str.size() - 1);
            }
            else if (!size_str.empty() && (size_str[size_str.size() - 1] == 'K' || size_str[size_str.size() - 1] == 'k'))
            {
                multiplier = 1024;
                size_str.erase(size_str.size() - 1);
            }
            server.client_max_body_size = std::atol(size_str.c_str()) * multiplier;
        }
    }
}

ServerConfig ConfigParser::getServerConfig(size_t index) const
{
    if (index < servers.size())
        return servers[index];
    return ServerConfig();
}