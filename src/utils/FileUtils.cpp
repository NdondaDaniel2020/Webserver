/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   FileUtils.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nmatondo <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/11/03 15:44:58 by nmatondo          #+#    #+#             */
/*   Updated: 2026/02/28 09:21:50 by nmatondo         ###   ########.fr       */
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

// ========== POST utilities ==========

bool createDirectory(const std::string& path)
{
    struct stat st;
    if (stat(path.c_str(), &st) == 0)
        return S_ISDIR(st.st_mode);
    
    // Criar diretório com permissões 755
    return mkdir(path.c_str(), 0755) == 0;
}

bool hasWritePermission(const std::string& path)
{
    return access(path.c_str(), W_OK) == 0;
}

std::string generateUniqueFilename(const std::string& original_name)
{
    // Timestamp + nome original
    time_t now = time(NULL);
    std::ostringstream oss;
    oss << now << "_" << original_name;
    return oss.str();
}

bool writeFileToDisk(const std::string& filepath, const std::string& content)
{
    std::ofstream file(filepath.c_str(), std::ios::binary | std::ios::out);
    if (!file.is_open())
        return false;
    
    file.write(content.c_str(), content.size());
    file.close();
    return true;
}

std::string urlDecode(const std::string& str)
{
    std::string result;
    for (size_t i = 0; i < str.size(); ++i)
    {
        if (str[i] == '%' && i + 2 < str.size())
        {
            // Converter hex para char
            int value = 0;
            std::istringstream iss(str.substr(i + 1, 2));
            iss >> std::hex >> value;
            result += static_cast<char>(value);
            i += 2;
        }
        else if (str[i] == '+')
        {
            result += ' ';
        }
        else
        {
            result += str[i];
        }
    }
    return result;
}

std::string extractBoundary(const std::string& content_type)
{
    size_t pos = content_type.find("boundary=");
    if (pos == std::string::npos)
        return "";
    
    std::string boundary = content_type.substr(pos + 9);
    
    // Remover espaços e aspas
    if (!boundary.empty() && boundary[0] == '"')
        boundary = boundary.substr(1);
    if (!boundary.empty() && boundary[boundary.size() - 1] == '"')
        boundary = boundary.substr(0, boundary.size() - 1);
    
    return boundary;
}

bool parseMultipartData(const std::string& body, const std::string& boundary, std::vector<MultipartFile>& files)
{

    if (boundary.empty())
        return false;
    
    std::string delimiter = "--" + boundary;
    std::string end_delimiter = "--" + boundary + "--";
    
    size_t pos = 0;
    while (pos < body.size())
    {
        // Encontrar início da parte
        pos = body.find(delimiter, pos);
        if (pos == std::string::npos)
            break;
        
        pos += delimiter.size();
        
        // Pular \r\n
        if (pos + 1 < body.size() && body[pos] == '\r' && body[pos + 1] == '\n')
            pos += 2;
        
        // Encontrar fim da parte
        size_t end_pos = body.find(delimiter, pos);
        if (end_pos == std::string::npos)
            break;
        
        // Extrair parte completa
        std::string part = body.substr(pos, end_pos - pos);
        
        // Separar headers e content
        size_t header_end = part.find("\r\n\r\n");
        if (header_end == std::string::npos)
            continue;
        
        std::string headers = part.substr(0, header_end);
        std::string content = part.substr(header_end + 4);
        
        // Remover \r\n do final do content
        if (content.size() >= 2 && content[content.size() - 2] == '\r')
            content = content.substr(0, content.size() - 2);
        
        // Extrair filename do Content-Disposition
        MultipartFile file;
        size_t filename_pos = headers.find("filename=\"");
        if (filename_pos != std::string::npos)
        {
            filename_pos += 10;
            size_t filename_end = headers.find("\"", filename_pos);
            if (filename_end != std::string::npos)
                file.filename = headers.substr(filename_pos, filename_end - filename_pos);
        }
        
        // Extrair Content-Type
        size_t ct_pos = headers.find("Content-Type: ");
        if (ct_pos != std::string::npos)
        {
            ct_pos += 14;
            size_t ct_end = headers.find("\r\n", ct_pos);
            if (ct_end == std::string::npos)
                ct_end = headers.size();
            file.content_type = headers.substr(ct_pos, ct_end - ct_pos);
        }
        
        file.content = content;
        
        if (!file.filename.empty())
            files.push_back(file);
        
        pos = end_pos;
    }
    
    return !files.empty();
}

// ========== File Validation ==========

std::string getFileExtension(const std::string& filename)
{
    size_t dot_pos = filename.find_last_of('.');
    if (dot_pos == std::string::npos || dot_pos == filename.size() - 1)
        return "";
    
    std::string ext = filename.substr(dot_pos);
    
    // Converter para lowercase
    for (size_t i = 0; i < ext.size(); ++i)
    {
        if (ext[i] >= 'A' && ext[i] <= 'Z')
            ext[i] = ext[i] + ('a' - 'A');
    }
    
    return ext;
}

bool isAllowedFileExtension(const std::string& filename, const std::vector<std::string>& allowed_extensions)
{
    if (allowed_extensions.empty())
        return true;  // Se não há lista, permite todos
    
    std::string file_ext = getFileExtension(filename);
    
    for (size_t i = 0; i < allowed_extensions.size(); ++i)
    {
        if (file_ext == allowed_extensions[i])
            return true;
    }
    
    return false;
}

void cleanupFiles(const std::vector<std::string>& file_paths)
{
    for (size_t i = 0; i < file_paths.size(); ++i)
    {
        if (remove(file_paths[i].c_str()) == 0)
        {
            std::cout << "[CLEANUP] Arquivo removido: " << file_paths[i] << std::endl;
        }
        else
        {
            std::cout << "[CLEANUP] Erro ao remover arquivo: " << file_paths[i] << std::endl;
        }
    }
}

std::string getParentDirectory(const std::string& path)
{
    size_t pos = path.find_last_of('/');
    if (pos != std::string::npos && pos > 0)
        return path.substr(0, pos);
    return ".";
}

size_t getFileSize(const std::string& path)
{
    struct stat st;
    if (stat(path.c_str(), &st) == 0)
        return st.st_size;
    return 0;
}

std::string getFileName(const std::string& path)
{
    size_t pos = path.find_last_of('/');
    if (pos == std::string::npos)
        return path;
    return path.substr(pos + 1);
}

std::string removeLocationInUri(const std::string& uri,  const LocationConfig* location)
{
    std::string uri_without_location = uri;
    if (location && !location->path.empty())
    {
        if (uri.find(location->path) == 0)
        {
            uri_without_location = uri.substr(location->path.length());
            if (uri_without_location.empty() || uri_without_location[0] != '/')
                uri_without_location = "/" + uri_without_location;
        }
    }
    return uri_without_location;
}