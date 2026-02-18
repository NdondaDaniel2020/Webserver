# include "../../include/ConfigParser.hpp"

ServerConfig ConfigParser::getServerConfig(size_t index) const
{
    if (index < servers.size())
        return servers[index];
    return ServerConfig();
}

void ConfigParser::parseRoot(const std::string& value, std::string& root)
{
    root = StringUtils::trim(value);
}

void ConfigParser::parseIndex(const std::string& value, std::vector<std::string>& index_files)
{
    std::string index_file;
    std::istringstream ivs(StringUtils::trim(value));

    while (ivs >> index_file)
        index_files.push_back(index_file);
}

void ConfigParser::parseClientMaxBodySize(const std::string& value, size_t& max_body_size)
{
    max_body_size = StringUtils::parseSize(value);
}

void ConfigParser::parseErrorPage(const std::string& value, std::map<std::string, std::string>& error_pages)
{
    std::string code;
    std::string path;
    std::istringstream evs(StringUtils::trim(value));

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