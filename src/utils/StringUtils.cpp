#include "StringUtils.hpp"
#include <sstream>

std::string StringUtils::trim(const std::string& str) 
{
    size_t first = str.find_first_not_of(" \t\r\n");
    size_t last = str.find_last_not_of(" \t\r\n");
    if (first == std::string::npos || last == std::string::npos)
        return "";
    return str.substr(first, last - first + 1);
}

size_t StringUtils::parseSize(std::string size_str)
{
    size_t multiplier = 1;

    size_str = trim(size_str);
    if (size_str.empty())
        return 0;

    char suffix = size_str[size_str.size() - 1];
    if (suffix == 'M' || suffix == 'm')
    {
        multiplier = 1024 * 1024;
        size_str.erase(size_str.size() - 1);
    }
    else if (suffix == 'K' || suffix == 'k')
    {
        multiplier = 1024;
        size_str.erase(size_str.size() - 1);
    }
    
    return std::atol(size_str.c_str()) * multiplier;
}

std::string StringUtils::parseUrlEncodedForm(const std::string &body)
{
    std::ostringstream result;
    std::string current = body;
    bool first = true;
    
    size_t pos = 0;
    while (pos < current.length())
    {
        size_t amp_pos = current.find('&', pos);
        if (amp_pos == std::string::npos)
            amp_pos = current.length();
        
        std::string pair = current.substr(pos, amp_pos - pos);
        
        size_t eq_pos = pair.find('=');
        if (eq_pos != std::string::npos)
        {
            std::string key = pair.substr(0, eq_pos);
            std::string value = pair.substr(eq_pos + 1);
            
            if (!first)
                result << ",";
            result << "\"" << key << "\":\"" << value << "\"";
            first = false;
        }
        
        pos = amp_pos + 1;
    }
    
    return result.str();
}
