# include "../../include/ConfigParser.hpp"

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
    extractTwoValues(value, code, path);
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
    std::string code_str;
    extractTwoValues(value, code_str, url);
    code = atoi(code_str.c_str());
}

void ConfigParser::parseCommonConfig(const std::string& key, const std::string& value, ServerConfig& server)
{
    if (key == "root")
        parseRoot(value, server.root);
    else if (key == "index")
        parseIndex(value, server.index_files);
    else if (key == "client_max_body_size")
        parseClientMaxBodySize(value, server.client_max_body_size);
}

void ConfigParser::parseCommonConfig(const std::string& key, const std::string& value, LocationConfig& location)
{
    if (key == "root")
        parseRoot(value, location.root);
    else if (key == "index")
        parseIndex(value, location.index_files);
    else if (key == "client_max_body_size")
        parseClientMaxBodySize(value, location.client_max_body_size);
}

void ConfigParser::extractKeyValue(const std::string& line, std::string& key, std::string& value)
{
    std::istringstream iss(line);
    iss >> key;
    std::getline(iss, value);
}

void ConfigParser::extractTwoValues(const std::string& input, std::string& first, std::string& second)
{
    std::istringstream iss(StringUtils::trim(input));
    iss >> first >> second;
}