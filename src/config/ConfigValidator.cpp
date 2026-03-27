#include "../include/ConfigValidator.hpp"

namespace ConfigValidator
{

    bool validatePort(std::string port)
    {
        port = StringUtils::trim(port);
        size_t pos = port.find(":");
        if (pos == std::string::npos)
            return false;
        std::string ip = port.substr(0, pos);
        if (ip.empty())
            return false;
        std::string number_port = port.substr(pos + 1);
        std::stringstream octs(ip);
        std::string oct;
        while (std::getline(octs, oct, '.'))
        {
            if (oct.size() > 3)
                return false;
            for (size_t i = 0; i < oct.size(); i++)
            {
                if (oct[i] != '*' && !isdigit(oct[i]))
                    return false;
            }
            int oct_int = atoi(oct.c_str());
            if (oct_int < 0 || oct_int > 254)
                return false;
        }
        if (port.empty())
            return false;
        for (size_t i = 0; i < number_port.size(); ++i)
        {
            if (!isdigit(number_port[i]))
                return false;
        }
        int port_num = atoi(number_port.c_str());
        return (port_num > 1023 && port_num <= 65535);
    }

    bool validateServerName(std::string &server_name)
    {
        server_name = StringUtils::trim(server_name);
        if (server_name.empty())
            return (false);

        bool has_alnum = false;
        for (size_t i = 0; i < server_name.size(); i++)
        {
            if (isalnum(server_name[i]))
            {
                has_alnum = true;
                break;
            }
        }
        return has_alnum;
    }

    bool validateReturn(std::string value)
    {
        value = StringUtils::trim(value);
        size_t pos = value.find(' ');
        if (pos == std::string::npos)
            return false;
        std::string num_red = value.substr(0, pos);
        std::string str_red = value.substr(pos + 1);
        for (size_t i = 0; i < num_red.size(); i++)
        {
            if (!isdigit(num_red[i]))
                return false;
        }
        if (str_red[0] != '/' && str_red[0] != 'h')
            return false;
        int num_redirect = atoi(num_red.c_str());
        if (num_redirect < 300 || num_redirect > 308)
            return false;
        return true;
    }

    bool validatePath(const std::string &path)
    {
        std::ifstream file(path.c_str());
        return file.good();
    }

    bool validateClientMaxBodySize(size_t size)
    {
        return (size > 0 && size <= 100000000);
    }

    bool validateHttpCode(const std::string &code)
    {
        if (code.size() != 3)
            return false;

        if (!isdigit(code[0]))
            return false;

        for (size_t i = 1; i < code.size(); i++)
        {
            if (!isdigit(code[i]) && code[i] != 'x' && code[i] != 'X')
                return false;
        }

        int first_digit = code[0] - '0';
        if (first_digit < 1 || first_digit > 5)
            return false;

        if (isdigit(code[1]) && isdigit(code[2]))
        {
            int int_code = atoi(code.c_str());
            return (int_code >= 100 && int_code <= 599);
        }

        return true;
    }
    bool validateAutoIndex(std::string value)
    {
        value = StringUtils::trim(value);
        if (value.empty())
            return false;
        return (value == "on" || value == "off");
    }
    bool validateConfigFile(const std::string &filename)
    {
        std::ifstream file(filename.c_str());
        return file.good();
    }

    bool validateServerConfig(const ServerConfig &server)
    {
        if (server.port <= 1024 || server.port > 65535)
            return false;
        if (server.server_name.empty())
            return false;
        if (server.root.empty())
            return false;
        if (!validateClientMaxBodySize(server.client_max_body_size))
            return false;
        for (size_t i = 0; i < server.locations.size(); i++)
        {
            if (!validateLocationConfig(server.locations[i]))
                return false;
        }
        return true;
    }
    bool validateRedirect(const LocationConfig &location)
    {
        if (location.redirect_code != 0 && (location.redirect_code < 300 || location.redirect_code > 399))
            return false;
        if (!location.redirect_url.empty() && location.redirect_code == 0)
            return false;
        return true;
    }
    bool validateCgiConfig(const LocationConfig &location)
    {
        if (!location.cgi_handlers.empty())
        {
            std::map<std::string, std::string>::const_iterator it;
            for (it = location.cgi_handlers.begin(); it != location.cgi_handlers.end(); ++it)
            {
                if (it->first.empty() || it->second.empty())
                    return false;
            }
        }
        return true;
    }
    bool validateUploadDir(const LocationConfig &location)
    {
        if (!location.upload_dir.empty())
        {
            struct stat st;
            if (stat(location.upload_dir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode))
                return false;
        }
        return true;
    }
    bool validateLocationConfig(const LocationConfig &location)
    {
        if (location.path.empty())
            return false;
        if (!validateRedirect(location))
            return false;
        if (!validateCgiConfig(location))
            return false;
        if (!validateUploadDir(location))
            return false;
        return true;
    }
    bool validateAllowedMethods(const LocationConfig &location)
    {
        for (size_t i = 0; i < location.allowed_methods.size(); i++)
        {
            if (location.allowed_methods[i] == "GET" ||
                location.allowed_methods[i] == "POST" ||
                location.allowed_methods[i] == "DELETE")
                return true;
        }
        return false;
    }
    bool validateConfig(const std::vector<ServerConfig> &servers)
    {
        for (size_t i = 0; i < servers.size(); i++)
        {
            if (!validateServerConfig(servers[i]))
                return false;
        }
        return true;
    }
    bool validateIndexFiles(const std::vector<std::string> &index_files)
    {
        for (size_t i = 0; i < index_files.size(); i++)
        {
            if (index_files[i].empty())
                return false;
        }
        return true;
    }
    bool validateErrorPages(const std::map<std::string, std::string> &error_pages)
    {
        std::map<std::string, std::string>::const_iterator it;
        for (it = error_pages.begin(); it != error_pages.end(); ++it)
        {
            if (!validateHttpCode(it->first))
                return false;
            if (it->second[0] != '/')
                return false;
            size_t pos = it->second.find("/");
            if (pos == std::string::npos)
                return false;
            if (it->second.empty())
                return false;
        }
        return true;
    }

    void validateServerConfigDefault(const ServerConfig& server)
    {
        if (!server.port || server.interface.empty())
            throw std::runtime_error("Missing required field: listen (IP/port)");

        if (server.port <= 1024)
            throw std::runtime_error("Invalid listen: port must be greater than 1024");

        if (server.server_name.empty())
            throw std::runtime_error("Missing required field: server_name");

        if (server.root.empty())
            throw std::runtime_error("Missing required field: root");

        if (server.client_max_body_size <= 0)
            throw std::runtime_error("Invalid client_max_body_size: must be greater than 0");
    }

    void validateLocationConfigDefault(const LocationConfig& location)
    {
        if (location.path.empty())
            throw std::runtime_error("Invalid location: path is required");

        if (location.redirect_code != 0)
        {
            if (location.redirect_url.empty())
                throw std::runtime_error("Invalid location redirect: redirect_url is required");
            return;
        }

        if (!location.cgi_handlers.empty())
        {
            if (location.cgi_path.empty())
                throw std::runtime_error("Invalid location CGI: cgi_handlers defined but cgi_path missing");
        }

        if (!location.upload_dir.empty())
        {
            bool hasPost = false;
            for (size_t i = 0; i < location.allowed_methods.size(); i++)
            {
                if (location.allowed_methods[i] == "POST")
                {
                    hasPost = true;
                    break;
                }
            }
            if (!hasPost)
                throw std::runtime_error("Invalid location: upload_dir defined but POST not in allowed_methods");
        }
    }

}
