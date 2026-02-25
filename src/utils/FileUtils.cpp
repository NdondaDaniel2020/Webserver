/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   FileUtils.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nmatondo <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/11/03 15:44:58 by nmatondo          #+#    #+#             */
/*   Updated: 2026/02/25 14:27:18 by nmatondo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "FileUtils.hpp"

std::string create_error_message(const std::string& error)
{
    std::ostringstream oss;
    oss << error << " - " << strerror(errno);
    return oss.str();
}

std::string readFile(const std::string& filepath)
{
    std::ifstream file(filepath.c_str());
    if (!file.is_open())
        return "";
    
    std::ostringstream buffer;
    buffer << file.rdbuf();
    file.close();
    
    return buffer.str();
}

void openFile(std::ifstream& file, const std::string& filename, std::string sms)
{
    file.open(filename.c_str());

    if (!file.is_open()) {
        throw std::runtime_error(sms + filename);
    }
}

// ========== Server ==========

uint32_t ipToHex(const std::string& ip)
{
    std::stringstream ss(ip);
    std::string part;

    std::getline(ss, part, '.');
    uint32_t a = static_cast<uint32_t>(std::atoi(part.c_str()));

    std::getline(ss, part, '.');
    uint32_t b = static_cast<uint32_t>(std::atoi(part.c_str()));

    std::getline(ss, part, '.');
    uint32_t c = static_cast<uint32_t>(std::atoi(part.c_str()));

    std::getline(ss, part, '.');
    uint32_t d = static_cast<uint32_t>(std::atoi(part.c_str()));

    return (a << 24) | (b << 16) | (c << 8) | d;
}

// ========== Path Security ==========

std::string normalizePath(const std::string& path)
{
    std::vector<std::string> parts;
    std::string result;
    std::string current;
    
    for (size_t i = 0; i < path.size(); ++i)
    {
        if (path[i] == '/')
        {
            if (!current.empty())
            {
                if (current == "..")
                {
                    if (!parts.empty())
                        parts.pop_back();
                }
                else if (current != ".")
                {
                    parts.push_back(current);
                }
                current.clear();
            }
        }
        else
        {
            current += path[i];
        }
    }
    
    if (!current.empty() && current != "." && current != "..")
        parts.push_back(current);
    
    for (size_t i = 0; i < parts.size(); ++i)
    {
        result += "/" + parts[i];
    }
    
    return result.empty() ? "/" : result;
}

std::string sanitizePath(const std::string& path)
{
    std::string normalized = normalizePath(path);
    
    // Remover múltiplas barras
    std::string result;
    bool lastWasSlash = false;
    
    for (size_t i = 0; i < normalized.size(); ++i)
    {
        if (normalized[i] == '/')
        {
            if (!lastWasSlash)
            {
                result += '/';
                lastWasSlash = true;
            }
        }
        else
        {
            result += normalized[i];
            lastWasSlash = false;
        }
    }
    
    return result;
}

std::string getRealPath(const std::string& path)
{
    char resolved_path[PATH_MAX];
    if (realpath(path.c_str(), resolved_path) != NULL)
        return std::string(resolved_path);
    return "";
}

bool isPathSafe(const std::string& path, const std::string& root)
{
    std::string real_path = getRealPath(path);
    std::string real_root = getRealPath(root);
    
    if (real_path.empty())
        return false;
    
    // Verificar se path começa com root
    return real_path.compare(0, real_root.size(), real_root) == 0;
}

// ========== File System Checks ==========

bool fileExists(const std::string& path)
{
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

bool isDirectory(const std::string& path)
{
    struct stat st;
    if (stat(path.c_str(), &st) != 0)
        return false;
    return S_ISDIR(st.st_mode);
}

bool isReadable(const std::string& path)
{
    return access(path.c_str(), R_OK) == 0;
}

std::string findIndexFile(const std::string& dir_path, const std::vector<std::string>& index_files)
{
    for (size_t i = 0; i < index_files.size(); ++i)
    {
        std::string full_path = dir_path;
        if (full_path[full_path.size() - 1] != '/')
            full_path += "/";
        full_path += index_files[i];
        
        if (fileExists(full_path) && !isDirectory(full_path) && isReadable(full_path))
            return full_path;
    }
    return "";
}

// ========== MIME Types ==========

std::string getMimeType(const std::string& path)
{
    size_t dot_pos = path.find_last_of('.');
    if (dot_pos == std::string::npos)
        return "application/octet-stream";
    
    std::string ext = path.substr(dot_pos);
    
    // HTML
    if (ext == ".html" || ext == ".htm") return "text/html";
    
    // CSS/JS
    if (ext == ".css") return "text/css";
    if (ext == ".js") return "application/javascript";
    if (ext == ".json") return "application/json";
    
    // Imagens
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".png") return "image/png";
    if (ext == ".gif") return "image/gif";
    if (ext == ".svg") return "image/svg+xml";
    if (ext == ".ico") return "image/x-icon";
    
    // Texto
    if (ext == ".txt") return "text/plain";
    if (ext == ".xml") return "application/xml";
    
    // PDFs e documentos
    if (ext == ".pdf") return "application/pdf";
    
    return "application/octet-stream";
}

// ========== HTTP Date ==========

std::string getCurrentHttpDate()
{
    time_t now = time(NULL);
    struct tm* tm_info = gmtime(&now);
    char buffer[100];
    
    strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S GMT", tm_info);
    return std::string(buffer);
}

std::string getFileModifiedDate(const std::string& path)
{
    struct stat st;
    if (stat(path.c_str(), &st) != 0)
        return getCurrentHttpDate();
    
    struct tm* tm_info = gmtime(&st.st_mtime);
    char buffer[100];
    
    strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S GMT", tm_info);
    return std::string(buffer);
}
