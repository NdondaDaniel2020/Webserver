#include "../../include/Response.hpp"
#include <algorithm>
#include <cctype>
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