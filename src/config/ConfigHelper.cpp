#include "ConfigHelper.hpp"

namespace ConfigHelper
{

    void parseRoot(const std::string &value, std::string &root)
    {
        root = StringUtils::trim(value);
    }
    void parseUploadDir(const std::string &value, std::string &upload_dir)
    {
        upload_dir = StringUtils::trim(value);
    }
    void parseIndex(const std::string &value, std::vector<std::string> &index_files)
    {
        std::string index_file;
        std::istringstream ivs(StringUtils::trim(value));

        while (ivs >> index_file)
            index_files.push_back(index_file);
    }

    void parseClientMaxBodySize(const std::string &value, size_t &max_body_size)
    {
        max_body_size = StringUtils::parseSize(value);
    }

    void parseErrorPage(const std::string &value, std::map<std::string, std::string> &error_pages)
    {
        std::string code;
        std::string path;
        Detail::extractTwoValues(value, code, path);
        error_pages[code] = path;
    }

    void parseAutoIndex(const std::string &value, bool &autoindex)
    {
        std::string val = StringUtils::trim(value);

        if (val == "on")
            autoindex = true;
        else
            autoindex = false;
    }

    void parseRedirect(const std::string &value, int &code, std::string &url)
    {
        std::string code_str;
        Detail::extractTwoValues(value, code_str, url);
        code = atoi(code_str.c_str());
    }

    void parseCgiTimeout(const std::string &value, time_t &cgi_timeout)
    {
        std::string trimmed = StringUtils::trim(value);
        
        if (trimmed.empty())
        {
            cgi_timeout = 0;
            return;
        }
        
        char unit = trimmed[trimmed.length() - 1];
        std::string num_str;
        
        if (unit == 'h' || unit == 'H')
        {
            num_str = trimmed.substr(0, trimmed.length() - 1);
            long val = std::atol(num_str.c_str());
            cgi_timeout = val * 3600;
        }
        else if (unit == 'm' || unit == 'M')
        {
            num_str = trimmed.substr(0, trimmed.length() - 1);
            long val = std::atol(num_str.c_str());
            cgi_timeout = val * 60;
        }
        else if (unit == 's' || unit == 'S')
        {
            num_str = trimmed.substr(0, trimmed.length() - 1);
            cgi_timeout = std::atol(num_str.c_str());
        }
        else
        {
            cgi_timeout = std::atol(trimmed.c_str());
        }
    }

    void parseTimeout(const std::string &value, time_t &timeout)
    {
        std::string trimmed = StringUtils::trim(value);
        
        if (trimmed.empty())
        {
            timeout = 0;
            return;
        }
        
        // Parsing idêntico ao parseCgiTimeout
        char unit = trimmed[trimmed.length() - 1];
        std::string num_str;
        
        if (unit == 'h' || unit == 'H')
        {
            num_str = trimmed.substr(0, trimmed.length() - 1);
            long val = std::atol(num_str.c_str());
            timeout = val * 3600;
        }
        else if (unit == 'm' || unit == 'M')
        {
            num_str = trimmed.substr(0, trimmed.length() - 1);
            long val = std::atol(num_str.c_str());
            timeout = val * 60;
        }
        else if (unit == 's' || unit == 'S')
        {
            num_str = trimmed.substr(0, trimmed.length() - 1);
            timeout = std::atol(num_str.c_str());
        }
        else
        {
            timeout = std::atol(trimmed.c_str());
        }
    }

    void parseCommonConfig(const std::string &key, const std::string &value, ServerConfig &server)
    {
        if (key == "root")
            parseRoot(value, server.root);
        else if (key == "index")
            parseIndex(value, server.index_files);
        else if (key == "client_max_body_size")
            parseClientMaxBodySize(value, server.client_max_body_size);
        else if (key == "cgi_timeout")
            parseCgiTimeout(value, server.cgi_timeout);
        else if (key == "timeout")
            parseTimeout(value, server.timeout);
        else
            throw std::runtime_error("invalid directive " + key);
    }

    void parseCommonConfig(const std::string &key, const std::string &value, LocationConfig &location)
    {
        if (key == "root")
            parseRoot(value, location.root);
        else if (key == "upload_dir")
            parseUploadDir(value, location.upload_dir);
        else if (key == "index")
            parseIndex(value, location.index_files);
        else if (key == "client_max_body_size")
            parseClientMaxBodySize(value, location.client_max_body_size);
        else if (key == "cgi_timeout")
            parseCgiTimeout(value, location.cgi_timeout);
        else if (key == "timeout")
            parseTimeout(value, location.timeout);
        else if (key == "allowed_methods")
        {
            std::istringstream iss(StringUtils::trim(value));
            std::string method;
            while (iss >> method)
            {
                if (method.empty() || (method != "GET" && method != "POST" && method != "DELETE"))
                    throw std::runtime_error("invalid HTTP method in allowed_methods: " + method);
                location.allowed_methods.push_back(method);
            }
        }
        else if (key == "cgi_extension" || key == "cgi_extensions")
        {
            std::string ext = StringUtils::trim(value);
            location.cgi_handlers[ext] = "";
            location.cgi_path = ext;
        }
        else if (key == "cgi_path")
        {
            // associa o path à última extensão lida
            std::string path = StringUtils::trim(value);
            if (!location.cgi_path.empty())
                location.cgi_handlers[location.cgi_path] = path;
            location.cgi_path = path;
        }
    }

    void extractKeyValue(const std::string &line, std::string &key, std::string &value)
    {
        std::istringstream iss(line);
        iss >> key;
        std::getline(iss, value);
    }

    namespace Detail
    {
        void extractTwoValues(const std::string &input, std::string &first, std::string &second)
        {
            std::istringstream iss(StringUtils::trim(input));
            iss >> first >> second;
        }
    }

}