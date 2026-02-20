#include "../include/ConfigValidator.hpp"

namespace ConfigValidator
{

    bool validatePort(std::string port)
    {
        int port_num;

        if (port.empty())
            return false;
        for (size_t i = 0; i < port.size(); ++i)
        {
            if (!isdigit(port[i]))
                return false;
        }
        port_num = atoi(port.c_str());
        return (port_num > 0 && port_num <= 65535);
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
        for (size_t i = 0; i < code.size(); i++)
        {
            if (!isdigit(code[i]))
                return false;
        }
        int int_code = atoi(code.c_str());
        return (int_code >= 100 && int_code <= 599);
    }
    bool validateAutoIndex(const std::string &value)
    {
        return (value == "on" || value == "off");
    }
    bool validateConfigFile(const std::string &filename)
    {
        std::ifstream file(filename.c_str());
        return file.good();
    }

    bool validateServerConfig(const ServerConfig& server)
    {
        if (server.port <= 0 || server.port > 65535)
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
    bool validateRedirect(const LocationConfig& location)
    {
        if (location.redirect_code != 0 && (location.redirect_code < 300 || location.redirect_code > 399))
            return false;
        if (!location.redirect_url.empty() && location.redirect_code == 0)
            return false;
        return true;
    }
    bool validateCgiConfig(const LocationConfig& location)
    {
        if (!location.cgi_path.empty() && location.cgi_extensions.empty())
            return false;
        if (location.cgi_path.empty() && !location.cgi_extensions.empty())
            return false;
        return true;
    }
    bool validateUploadDir(const LocationConfig& location)
    {
        if (!location.upload_dir.empty())
        {
            struct stat st;
            if (stat(location.upload_dir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode))
                return false;
        }
        return true;
    }
    bool validateLocationConfig(const LocationConfig& location)
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
                location.allowed_methods[i] == "DELETE" ) 
                   return true;
        }
        return false;
    }
        bool validateConfig(const std::vector<ServerConfig>& servers)
        {
            for (size_t i = 0; i < servers.size(); i++)
            {
                if (!validateServerConfig(servers[i]))
                    return false;
            }
            return true;
        }
        bool validateIndexFiles(const std::vector<std::string>& index_files)
        {
            for (size_t i = 0; i < index_files.size(); i++)
            {
                if (index_files[i].empty())
                    return false;
            }
            return true;
        }
        bool validateErrorPages(const std::map<std::string, std::string>& error_pages)
        {
            std::map<std::string, std::string>::const_iterator it;
            for (it = error_pages.begin(); it != error_pages.end(); ++it)
            {
                if (!validateHttpCode(it->first))
                    return false;
                if (it->second.empty())
                    return false;
            }
            return true;
        }
}

