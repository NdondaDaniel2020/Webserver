/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Response.cpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nmatondo <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/03 10:05:33 by nmatondo          #+#    #+#             */
/*   Updated: 2026/03/22 17:37:44 by nmatondo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Response.hpp"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <sys/stat.h>

namespace
{
    struct DirectoryEntry
    {
        std::string name;
        bool is_directory;
        off_t size;
        time_t mtime;
    };

    static bool directoryEntryLess(const DirectoryEntry &a, const DirectoryEntry &b)
    {
        if (a.name == "..")
            return true;
        if (b.name == "..")
            return false;
        if (a.is_directory != b.is_directory)
            return a.is_directory;
        return a.name < b.name;
    }

    static std::string htmlEscape(const std::string &value)
    {
        std::string escaped;

        for (size_t i = 0; i < value.size(); ++i)
        {
            if (value[i] == '&')
                escaped += "&amp;";
            else if (value[i] == '<')
                escaped += "&lt;";
            else if (value[i] == '>')
                escaped += "&gt;";
            else if (value[i] == '"')
                escaped += "&quot;";
            else
                escaped += value[i];
        }
        return escaped;
    }

    static std::string encodeUriSegment(const std::string &name)
    {
        std::ostringstream encoded;
        const char *hex = "0123456789ABCDEF";

        for (size_t i = 0; i < name.size(); ++i)
        {
            unsigned char c = static_cast<unsigned char>(name[i]);

            if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
                encoded << name[i];
            else
            {
                encoded << '%';
                encoded << hex[c >> 4];
                encoded << hex[c & 0x0F];
            }
        }
        return encoded.str();
    }

    static std::string normalizeDirectoryUri(const std::string &uri)
    {
        std::string normalized = uri;

        if (normalized.empty() || normalized[0] != '/')
            normalized = "/" + normalized;
        if (normalized[normalized.size() - 1] != '/')
            normalized += '/';
        return normalized;
    }

    static std::string parentDirectoryUri(const std::string &uri)
    {
        std::string normalized = normalizeDirectoryUri(uri);

        if (normalized == "/")
            return "/";

        size_t end = normalized.size() - 1;
        size_t pos = normalized.rfind('/', end - 1);

        if (pos == std::string::npos)
            return "/";
        if (pos == 0)
            return "/";
        return normalized.substr(0, pos + 1);
    }

}

Response::Response(const HttpRequest &request, const ServerConfig &config) : config(config)
{
    this->allowed_extensions.push_back(".jpg");
    this->allowed_extensions.push_back(".jpeg");
    this->allowed_extensions.push_back(".png");
    this->allowed_extensions.push_back(".gif");
    this->allowed_extensions.push_back(".pdf");
    this->allowed_extensions.push_back(".txt");
    this->allowed_extensions.push_back(".doc");
    this->allowed_extensions.push_back(".docx");
    this->allowed_extensions.push_back(".zip");
    this->allowed_extensions.push_back(".mp4");
    this->allowed_extensions.push_back(".mp3");

    this->protected_files.push_back("index.html");

    buildHttpResponse(request);
}

Response::~Response()
{
}

Response::Response(const Response &other)
{
    *this = other;
}

Response &Response::operator=(const Response &other)
{
    if (this != &other)
    {
        this->response_str = other.response_str;
        this->config = other.config;
        this->protected_files = other.protected_files;
        this->allowed_extensions = other.allowed_extensions;
    }
    return *this;
}

std::string Response::getResponseHttp()
{
    return this->response_str;
}

void Response::buildHttpResponse(const HttpRequest &request)
{
    std::string uri = sanitizePath(request.getUri());
    std::string root = this->config.root;
    const LocationConfig *location = findMatchingLocation(request.getUri());

    if (location && !location->root.empty())
        root = location->root;

    std::string file_path = root + removeLocationInUri(uri, location);

    if (!validateAllowedMethod(request))
    {
        std::cout << "[405] Método " << request.getMethod() << " não permitido para: " << request.getUri() << std::endl;
        return StatusCodes::http405MethodNotAllowed(this->response_str, request, file_path, this->config);
    }

    if (request.getMethod() == "GET")
        methodGet(request, file_path);
    else if (request.getMethod() == "POST")
        methodPost(request);
    else if (request.getMethod() == "DELETE")
        methodDelete(request, file_path);
    else
        StatusCodes::http405MethodNotAllowed(this->response_str, request, file_path, this->config);
}

void Response::methodGet(const HttpRequest &request, const std::string &file_path)
{
    std::string _file_path = file_path;

    const LocationConfig *location = findMatchingLocation(request.getUri());
    if (location && !location->cgi_handlers.empty())
        return StatusCodes::http502BadGateway(this->response_str, request, "Fail CGI", this->config);

    if (location && location->redirect_code > 0)
        return handleRedirect(location->redirect_code, location->redirect_url);
    if (isDirectory(_file_path))
    {
        std::vector<std::string> index_files = this->config.index_files;
        if (location && !location->index_files.empty())
            index_files = location->index_files;
        std::string index_path = findIndexFile(_file_path, index_files);
        if (!index_path.empty())
            _file_path = index_path;
        else
        {
            if (location && location->autoindex)
                return generateDirectoryListing(request, file_path, request.getUri());
            else
                return StatusCodes::http403Forbidden(this->response_str, request, _file_path, this->config);
        }
    }
    if (!fileExists(_file_path))
    {
        return StatusCodes::http404NotFound(
            this->response_str,
            request,
            _file_path,
            this->config);
    }
    if (!isReadable(_file_path))
        return StatusCodes::http403Forbidden(this->response_str, request, _file_path, this->config);
    std::string content = readFile(_file_path);
    StatusCodes::http200FileFound(
        this->response_str,
        request,
        content,
        _file_path);
}

void Response::methodPost(const HttpRequest &request)
{
    size_t body_size = request.getBody().size();

    if (this->config.client_max_body_size > 0 && body_size > this->config.client_max_body_size)
    {
        std::cout << "[413] Body size (" << body_size << ") excede limite do servidor ("
                  << this->config.client_max_body_size << ")" << std::endl;
        return StatusCodes::http413PayloadTooLarge(this->response_str, this->config);
    }

    const LocationConfig *location = findMatchingLocation(request.getUri());
    if (location && !location->cgi_handlers.empty())
        return StatusCodes::http502BadGateway(this->response_str, request, "Fail CGI", this->config);

    if (location && location->client_max_body_size > 0 && body_size > location->client_max_body_size)
    {
        std::cout << "[413] Body size (" << body_size << ") excede limite da location ("
                  << location->client_max_body_size << ")" << std::endl;
        return StatusCodes::http413PayloadTooLarge(this->response_str, this->config);
    }

    std::string content_type = request.getHeader("Content-Type");
    if (content_type.empty())
    {
        std::cout << "[400] Content-Type header é obrigatório" << std::endl;
        return StatusCodes::http400BadRequest(this->response_str, "Content-Type header is required", this->config);
    }

    if (content_type.find("multipart/form-data") != std::string::npos)
        return multipartFormData(request, content_type);

    else if (content_type.find("application/x-www-form-urlencoded") != std::string::npos)
    {
        std::string decoded_body = urlDecode(request.getBody());

        std::ostringstream json_response;
        json_response << "{\"message\":\"Form data recebido\",";
        json_response << "\"size\":" << decoded_body.size() << "}";

        return StatusCodes::http200Ok(this->response_str, json_response.str());
    }
    else if (content_type.find("application/json") != std::string::npos)
    {
        std::string json_body = request.getBody();

        std::ostringstream json_response;
        json_response << "{\"message\":\"JSON recebido\",";
        json_response << "\"size\":" << json_body.size() << "}";

        return StatusCodes::http200Ok(this->response_str, json_response.str());
    }
    else if (content_type.find("text/plain") != std::string::npos)
    {
        std::ostringstream json_response;
        json_response << "{\"message\":\"Text data recebido\",";
        json_response << "\"size\":" << request.getBody().size() << "}";

        return StatusCodes::http200Ok(this->response_str, json_response.str());
    }
    else
    {
        std::cout << "[415] Content-Type não suportado: " << content_type << std::endl;
        return StatusCodes::http415UnsupportedMediaType(this->response_str, request, this->config);
    }
}

void Response::methodDelete(const HttpRequest &request, const std::string &file_path)
{
    if (!fileExists(file_path))
    {
        std::cout << "[404] Arquivo não encontrado para deletar: " << file_path << std::endl;
        return StatusCodes::http404NotFound(this->response_str, request, file_path, this->config);
    }

    if (isDirectory(file_path))
    {
        std::cout << "[403] Não é permitido deletar diretórios: " << file_path << std::endl;
        return StatusCodes::http403Forbidden(this->response_str, request, file_path, this->config);
    }

    std::string real_path = getRealPath(file_path);
    if (real_path.empty())
    {
        std::cout << "[403] Não foi possível resolver path real: " << file_path << std::endl;
        return StatusCodes::http403Forbidden(this->response_str, request, file_path, this->config);
    }

    std::string root = this->config.root;
    const LocationConfig *location = findMatchingLocation(request.getUri());
    if (location && !location->root.empty())
        root = location->root;

    if (!isPathSafe(real_path, root))
    {
        std::cout << "[403] Symlink aponta para fora do root permitido: " << file_path << " -> " << real_path << std::endl;
        return StatusCodes::http403Forbidden(this->response_str, request, file_path, this->config);
    }

    std::string parent_dir = getParentDirectory(file_path);
    if (!hasWritePermission(parent_dir))
    {
        std::cout << "[403] Sem permissão para deletar: " << file_path << std::endl;
        return StatusCodes::http403Forbidden(this->response_str, request, file_path, this->config);
    }

    std::string filename = getFileName(file_path);
    if (isProtectedFile(filename))
    {
        std::cout << "[403] Arquivo protegido, não pode ser deletado: " << file_path << std::endl;
        return StatusCodes::http403Forbidden(this->response_str, request, file_path, this->config);
    }

    size_t file_size = getFileSize(file_path);
    std::cout << "[DELETE] Arquivo: " << file_path << std::endl;
    std::cout << "[DELETE] Tamanho: " << file_size << " bytes" << std::endl;
    std::cout << "[DELETE] URI: " << request.getUri() << std::endl;

    if (remove(file_path.c_str()) != 0)
    {
        std::cout << "[500] Erro ao deletar arquivo: " << strerror(errno) << std::endl;
        return StatusCodes::http500InternalServerError(this->response_str, request, "Failed to delete file: " + std::string(strerror(errno)), this->config);
    }

    std::cout << "[DELETE] ✓ Arquivo deletado com sucesso" << std::endl;
    return StatusCodes::http204NoContent(this->response_str);
}

void Response::multipartFormData(const HttpRequest &request, const std::string &content_type)
{
    std::string boundary = extractBoundary(content_type);
    if (boundary.empty())
    {
        std::cout << "[400] Boundary não encontrado no Content-Type" << std::endl;
        return StatusCodes::http415UnsupportedMediaType(this->response_str, request, this->config);
    }

    std::vector<MultipartFile> files;
    if (!parseMultipartData(request.getBody(), boundary, files))
    {
        std::cout << "[400] Erro ao parsear multipart data" << std::endl;
        return StatusCodes::http415UnsupportedMediaType(this->response_str, request, this->config);
    }

    std::string upload_dir = getUploadDir(request);
    if (upload_dir.empty())
        return StatusCodes::http413PayloadTooLarge(this->response_str, this->config);

    if (!createDirectory(upload_dir))
    {
        std::cout << "[500] Erro ao criar diretório de upload: " << upload_dir << std::endl;
        return StatusCodes::http500InternalServerError(this->response_str, request, "Failed to create upload directory: " + upload_dir, this->config);
    }

    if (!hasWritePermission(upload_dir))
    {
        std::cout << "[403] Sem permissão de escrita em: " << upload_dir << std::endl;
        return StatusCodes::http403Forbidden(this->response_str, request, upload_dir, this->config);
    }

    std::ostringstream json_response;
    json_response << "{\"files\":[";
    std::vector<std::string> saved_files;
    size_t success_count = 0;

    for (size_t i = 0; i < files.size(); ++i)
    {
        if (!isAllowedFileExtension(files[i].filename, this->allowed_extensions))
        {
            std::cout << "[400] Extensão de arquivo não permitida: " << files[i].filename << std::endl;
            cleanupFiles(saved_files); // Limpar arquivos já salvos
            return StatusCodes::http400BadRequest(this->response_str, "File extension not allowed: " + getFileExtension(files[i].filename), this->config);
        }

        if (files[i].content.size() > 10 * 1024 * 1024)
        {
            std::cout << "[413] Arquivo muito grande: " << files[i].filename
                      << " (" << files[i].content.size() << " bytes)" << std::endl;
            cleanupFiles(saved_files);
            return StatusCodes::http413PayloadTooLarge(this->response_str, this->config);
        }

        std::string unique_filename = generateUniqueFilename(files[i].filename);
        std::string full_path = upload_dir + "/" + unique_filename;

        if (writeFileToDisk(full_path, files[i].content))
        {
            saved_files.push_back(full_path);
            std::cout << "[201] Arquivo salvo: " << full_path
                      << " (" << files[i].content.size() << " bytes)" << std::endl;

            if (success_count > 0)
                json_response << ",";

            json_response << "{";
            json_response << "\"filename\":\"" << unique_filename << "\",";
            json_response << "\"original_name\":\"" << files[i].filename << "\",";
            json_response << "\"path\":\"" << full_path << "\",";
            json_response << "\"size\":" << files[i].content.size() << ",";
            json_response << "\"mime_type\":\"" << getMimeType(files[i].filename) << "\"";
            json_response << "}";
            success_count++;
        }
        else
        {
            std::cout << "[500] Erro ao salvar arquivo: " << full_path << std::endl;
            cleanupFiles(saved_files); // Limpar todos em caso de erro
            return StatusCodes::http400BadRequest(this->response_str, "Failed to save file: " + files[i].filename, this->config);
        }
    }

    json_response << "],\"success\":true,\"count\":" << success_count << "}";

    std::string location = "";
    if (!saved_files.empty())
    {
        std::string first_file = saved_files[0];
        size_t upload_pos = first_file.find("/uploads/");
        if (upload_pos != std::string::npos)
            location = first_file.substr(upload_pos);
        else
            location = request.getUri() + "/" + getFileName(first_file);
    }

    StatusCodes::http201Created(this->response_str, location, json_response.str());
}

const LocationConfig *Response::findMatchingLocation(const std::string &uri) const
{
    const LocationConfig *best_match = NULL;
    size_t best_match_length = 0;

    for (size_t i = 0; i < this->config.locations.size(); i++)
    {
        const std::string &location_path = this->config.locations[i].path;

        if (uri.find(location_path) == 0)
        {
            if (location_path.size() > best_match_length)
            {
                best_match = &this->config.locations[i];
                best_match_length = location_path.size();
            }
        }
    }

    return best_match;
}

bool Response::validateAllowedMethod(const HttpRequest &request)
{
    const LocationConfig *location = findMatchingLocation(request.getUri());

    if (!location)
        return request.getMethod() == "GET";

    if (location && !location->allowed_methods.empty())
    {
        std::vector<std::string>::const_iterator it = std::find(
            location->allowed_methods.begin(),
            location->allowed_methods.end(),
            request.getMethod());

        if (it == location->allowed_methods.end())
            return false;
    }
    return true;
}

std::string Response::getUploadDir(const HttpRequest &request)
{
    const LocationConfig *location = findMatchingLocation(request.getUri());

    if (location && location->client_max_body_size > 0 &&
        request.getBody().size() > location->client_max_body_size)
    {
        std::cout << "[413] Body size (" << request.getBody().size()
                  << ") excede limite da location ("
                  << location->client_max_body_size << ")" << std::endl;
        return "";
    }

    std::string upload_dir = this->config.root;

    if (location && !location->upload_dir.empty())
    {
        if (location->upload_dir[0] != '/')
            upload_dir = this->config.root + "/" + location->upload_dir;
        else
            upload_dir = location->upload_dir;
    }

    return upload_dir;
}

void Response::generateDirectoryListing(const HttpRequest &request, const std::string &dir_path, const std::string &uri)
{
    DIR *dir = opendir(dir_path.c_str());
    if (!dir)
    {
        StatusCodes::http500InternalServerError(this->response_str, request, "Failed to open directory", this->config);
        return;
    }

    std::vector<DirectoryEntry> entries;
    struct dirent *entry = NULL;

    try
    {
        while ((entry = readdir(dir)) != NULL)
        {
            std::string name = entry->d_name;

            if (name == ".")
                continue;

            DirectoryEntry item;
            item.name = name;
            item.is_directory = false;
            item.size = 0;
            item.mtime = 0;

            if (name == "..")
            {
                item.is_directory = true;
                entries.push_back(item);
                continue;
            }

            std::string full_path = dir_path;
            if (!full_path.empty() && full_path[full_path.size() - 1] != '/')
                full_path += '/';
            full_path += name;

            struct stat file_stat;
            if (stat(full_path.c_str(), &file_stat) == 0)
            {
                item.is_directory = S_ISDIR(file_stat.st_mode);
                item.size = file_stat.st_size;
                item.mtime = file_stat.st_mtime;
            }

            entries.push_back(item);
        }

        std::sort(entries.begin(), entries.end(), directoryEntryLess);

        const std::string current_uri = normalizeDirectoryUri(uri);
        const std::string parent_uri = parentDirectoryUri(current_uri);

        std::ostringstream html;
        html << "<!DOCTYPE html>\n<html lang=\"pt\">\n<head>\n";
        html << "    <meta charset=\"UTF-8\">\n";
        html << "    <title>Index of " << htmlEscape(current_uri) << " | Web Ninjas</title>\n";
        html << "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";
        html << "    <link href=\"https://fonts.googleapis.com/css2?family=Orbitron:wght@400;700&family=Inter:wght@300;400;500;600&display=swap\" rel=\"stylesheet\">\n";
        html << "    <link href=\"/assets/directory-listing.css\" rel=\"stylesheet\">\n";
        html << "</head>\n<body>\n";
        html << "    <div class=\"background-lines\"></div>\n";
        html << "    <main>\n";
        html << "        <div class=\"header\">\n";
        html << "            <h1>📁 " << htmlEscape(current_uri) << "</h1>\n";
        html << "            <p>Directory Contents</p>\n";
        html << "        </div>\n";
        html << "        <div class=\"breadcrumb\">\n";
        html << "            <a href=\"/\">~/</a>\n";

        // Gerar breadcrumb
        {
            std::string path = current_uri;
            std::string acc = "";
            if (current_uri != "/") {
                size_t pos = 0;
                while ((pos = path.find('/', pos + 1)) != std::string::npos) {
                    acc = path.substr(0, pos);
                    html << "<a href=\"" << htmlEscape(acc) << "\">" << htmlEscape(acc.substr(acc.rfind('/') + 1)) << "/</a> ";
                }
            }
        }

        html << "        </div>\n";
        html << "        <div class=\"files-container\">\n";

        // Adicionar link para parent directory
        if (current_uri != "/")
        {
            html << "            <a href=\"" << htmlEscape(parent_uri) << "\" class=\"file-card parent-dir\">\n";
            html << "                <div class=\"file-icon\">⬆️</div>\n";
            html << "                <span class=\"file-name\">..</span>\n";
            html << "                <span class=\"file-type\">Parent Directory</span>\n";
            html << "            </a>\n";
        }

        // Adicionar arquivos e diretórios
        bool has_files = false;
        for (size_t i = 0; i < entries.size(); ++i)
        {
            if (entries[i].name == "..")
                continue;
            has_files = true;
            break;
        }

        if (!has_files)
        {
            html << "        </div>\n";
            html << "        <div class=\"empty-state\">\n";
            html << "            <div class=\"empty-state-icon\">📭</div>\n";
            html << "            <h2>No files found</h2>\n";
            html << "            <p>This directory is empty</p>\n";
            html << "        </div>\n";
        }
        else
        {
            for (size_t i = 0; i < entries.size(); ++i)
            {
                if (entries[i].name == "..")
                    continue;

                std::string icon = entries[i].is_directory ? "📂" : "📄";
                std::string file_type = entries[i].is_directory ? "Directory" : "File";
                std::string href = current_uri + encodeUriSegment(entries[i].name);
                if (entries[i].is_directory)
                    href += '/';

                html << "            <a href=\"" << htmlEscape(href) << "\" class=\"file-card\">\n";
                html << "                <div class=\"file-icon\">" << icon << "</div>\n";
                html << "                <span class=\"file-name\">" << htmlEscape(entries[i].name) << "</span>\n";
                html << "                <span class=\"file-type\">" << file_type << "</span>\n";
                html << "            </a>\n";
            }
            html << "        </div>\n";
        }

        html << "    </main>\n";
        html << "    <footer>\n";
        html << "        <p>🥷 <strong>Web Ninjas</strong> — HTTP Server @ 42 Project</p>\n";
        html << "    </footer>\n";
        html << "</body>\n</html>\n";

        closedir(dir);
        StatusCodes::http200FileFound(this->response_str, request, html.str(), dir_path);
    }
    catch (...)
    {
        closedir(dir);
        StatusCodes::http500InternalServerError(this->response_str, request, "Error generating directory listing", this->config);
        return;
    }
}

void Response::handleRedirect(int code, const std::string &url)
{
    std::ostringstream oss;
    oss << "HTTP/1.1 " << code;

    if (code == 301)
        oss << " Moved Permanently\r\n";
    else if (code == 302)
        oss << " Found\r\n";
    else if (code == 307)
        oss << " Temporary Redirect\r\n";
    else if (code == 308)
        oss << " Permanent Redirect\r\n";

    oss << "Location: " << url << "\r\n";
    oss << "Content-Length: 0\r\n";
    oss << "Connection: close\r\n\r\n";
    this->response_str = oss.str();
}

bool Response::isProtectedFile(const std::string &filename)
{
    for (size_t i = 0; i < this->protected_files.size(); ++i)
    {
        if (filename == this->protected_files[i])
            return true;
    }
    return false;
}

std::string Response::removeLocationInUri(const std::string &uri, const LocationConfig *location) const
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
