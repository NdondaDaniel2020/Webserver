
# include "../../include/ConfigParser.hpp"
# include "../../include/ConfigHelper.hpp"
# include    "../../include/ConfigValidator.hpp"

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
            continue ;
        
        if (line.find("server") != std::string::npos)
        {
            size_t serverPos = line.find("server");
            size_t bracePos = line.find("{");

            if (serverPos != 0)
                throw std::runtime_error("Invalid server declaration: 'server' must be at the beginning of the line");
            
            std::string openingBrace = line;
            
            if (bracePos == std::string::npos)
            {
                std::string afterServer = line.substr(6);
                afterServer = StringUtils::trim(afterServer);
                if (!afterServer.empty())
                    throw std::runtime_error("Invalid server declaration: unexpected text between 'server' and '{'");
                
                bool foundOpening = false;
                while (std::getline(file, openingBrace))
                {
                    openingBrace = StringUtils::trim(openingBrace);
                    if (!openingBrace.empty() && openingBrace[0] != '#')
                    {
                        foundOpening = true;
                        break;
                    }
                }
                if (!foundOpening || openingBrace != "{")
                    throw std::runtime_error("Expected '{' after 'server' declaration");
            }
            else
            {
                std::string between = line.substr(6, bracePos - 6); // entre "server" e "{"
                between = StringUtils::trim(between);
                if (!between.empty())
                    throw std::runtime_error("Invalid server declaration: unexpected text between 'server' and '{'");
            }
            
            parseServerBlock(file, config);
            servers.push_back(config);
            config = ServerConfig();
        }
        else
            throw std::runtime_error("Invalid configuration: unexpected content outside server block: " + line);
    }
    file.close();
    if (servers.empty())
    {
        throw std::runtime_error("No server configurations found in file: " + filename);
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
            continue ;

        if (line == "}")
            break ;

        if (line[line.size() - 1] != ';' && line.find("location") == std::string::npos)
            throw std::runtime_error("Missing semicolon: " + line);
        if (!line.empty() && line[line.size() - 1] == ';')
            line.erase(line.size() - 1);
        
        std::string key, value;
        ConfigHelper::extractKeyValue(line, key, value);

        if (!key.size()  || !value.size())
            throw std::runtime_error("Invalid key or value: " + line);

        if (key == "listen")
        {
            if (ConfigValidator::validatePort(value))
            {
                size_t pos = value.find(":");
                if (value.substr(0, pos).size() == 1)
                    server.interface = "0.0.0.0";
                server.interface = StringUtils::trim(value.substr(0, pos));
                server.port = atoi(StringUtils::trim(value.substr(pos +1)).c_str());
            }
            else
                throw std::runtime_error("invalid interface:port " + line);
        }
        else if (key == "server_name")
        {
            if (ConfigValidator::validateServerName(value))
                server.server_name = StringUtils::trim(value);
            else 
                throw std::runtime_error("invalid server name " + line);
        }
        else if (key == "error_page")
        {
            if (ConfigValidator::validateErrorPages(server.error_pages))
                ConfigHelper::parseErrorPage(value, server.error_pages);
            else
                throw std::runtime_error("invalid error page arguments "+ line );
        }
        else if (key == "location")
        {
            LocationConfig location;
            std::string path = StringUtils::trim(value);
            
            // Remove "{" do final se existir
            size_t bracePos = path.find('{');
            
            if (bracePos != std::string::npos)
            {
                // Extrai o path antes de "{"
                std::string beforeBrace = path.substr(0, bracePos);
                beforeBrace = StringUtils::trim(beforeBrace);
                
                if (beforeBrace.empty())
                    throw std::runtime_error("Invalid location declaration: location must have a path");
                
                // Valida que entre path e "{" só há whitespace
                std::string between = path.substr(beforeBrace.length(), bracePos - beforeBrace.length());
                // Não trima, deixa como está - ser whitespace é válido
                for (size_t i = 0; i < between.length(); i++)
                {
                    if (!std::isspace(between[i]))
                        throw std::runtime_error("Invalid location declaration: unexpected text between path and '{'");
                }
                
                path = beforeBrace;
            }
            else
            {
                // Sem "{" na mesma linha, procura a próxima linha
                if (path.empty())
                    throw std::runtime_error("Invalid location declaration: location must have a path");
                
                std::string nextLine;
                bool foundBrace = false;
                
                while (std::getline(file, nextLine))
                {
                    nextLine = StringUtils::trim(nextLine);
                    if (!nextLine.empty() && nextLine[0] != '#')
                    {
                        if (nextLine == "{")
                        {
                            foundBrace = true;
                        }
                        break;
                    }
                }
                
                if (!foundBrace)
                    throw std::runtime_error("Invalid location declaration: expected '{' after 'location " + path + "'");
            }
            
            location.path = path;
            parseLocationBlock(file, location);
            server.locations.push_back(location);
        }
        else
            ConfigHelper::parseCommonConfig(key, value, server);
    }
    ConfigValidator::validateServerConfigDefault(server);
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
        
        // Valida que a linha tem semicolon no final (exceto para blocos nested)
        if (line[line.size() - 1] != ';' && line.find("location") == std::string::npos)
            throw std::runtime_error("Missing semicolon: " + line);
        
        if (!line.empty() && line[line.size() - 1] == ';')
            line.erase(line.size() - 1);
        
        std::string key, value;
        ConfigHelper::extractKeyValue(line, key, value);
        
        // Valida que key e value não estão vazios
        if (key.empty() && !value.empty())
            throw std::runtime_error("Invalid location configuration: invalid key in line: " + line);
        
        if (key == "autoindex")
        {
           if (ConfigValidator::validateAutoIndex(value))
                ConfigHelper::parseAutoIndex(value, location.autoindex);
            else
                throw std::runtime_error("invalid arguments " + line);
        }
        else if (key == "return")
        {
            if (ConfigValidator::validateReturn(value))
                ConfigHelper::parseRedirect(value, location.redirect_code, location.redirect_url);
            else
                throw std::runtime_error("invalid return " + line);
        }
        else if (!key.empty())
            ConfigHelper::parseCommonConfig(key, value, location);
        else
            throw std::runtime_error("Invalid location configuration: " + line);
    }
}

