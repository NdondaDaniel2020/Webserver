/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Response.cpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nmatondo <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/03 10:05:33 by nmatondo          #+#    #+#             */
/*   Updated: 2026/03/04 11:25:20 by nmatondo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Response.hpp"

Response::Response(const HttpRequest &request, const ServerConfig &config) : config(config)
{
    // Extensões de arquivo permitidas em post (segurança)
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

bool Response::isCgiRequest(const std::string &uri,
                            const LocationConfig *location)
{
    if (!location)
        return false;

    if (location->cgi_handlers.empty())
        return false;

    size_t pos = uri.rfind('.');

    if (pos == std::string::npos)
        return false;

    std::string ext = uri.substr(pos);

    if (location->cgi_handlers.find(ext) != location->cgi_handlers.end())
        return true;

    return false;
}

void Response::buildHttpResponse(const HttpRequest &request)
{
    // 1. Sanitizar URI para prevenir path traversal
    std::string uri = sanitizePath(request.getUri());

    // Determinar root (location pode sobrescrever)
    std::string root = this->config.root;
    const LocationConfig *location = findMatchingLocation(request.getUri());

    if (location && !location->root.empty())
        root = location->root;

    std::string file_path = root + removeLocationInUri(uri, location);
    
    // 2. Validar se método é permitido por location
    if (!validateAllowedMethod(request))
    {
        std::cout << "[405] Método " << request.getMethod() << " não permitido para: " << request.getUri() << std::endl;
        return StatusCodes::http405MethodNotAllowed(this->response_str, file_path);
    }

    // 3. Executar método
    if (request.getMethod() == "GET")
        methodGet(request, file_path);
    else if (request.getMethod() == "POST")
        methodPost(request);
    else if (request.getMethod() == "DELETE")
        methodDelete(request, file_path);
    else
        StatusCodes::http405MethodNotAllowed(this->response_str, file_path);
}

void Response::methodGet(const HttpRequest &request, const std::string &file_path)
{
    std::string _file_path = file_path;

    const LocationConfig *location = findMatchingLocation(request.getUri());

    // ---------- CGI ----------
    if (isCgiRequest(request.getUri(), location))
    {
        std::string cgi_output;

        if (CGIHandler::executeCgi(request, _file_path, *location, cgi_output))
        {
            this->response_str = cgi_output;
            return;
        }
        else
        {
            StatusCodes::http500InternalServerError(
                this->response_str,
                "CGI execution failed");
            return;
        }
    }

    // ---------- redirect ----------
    if (location && location->redirect_code > 0)
        return handleRedirect(location->redirect_code, location->redirect_url);

    // ---------- diretório ----------
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
                return StatusCodes::http403Forbidden(this->response_str, _file_path);
        }
    }

    // ---------- arquivo existe ----------
    if (!fileExists(_file_path))
    {
        return StatusCodes::http404NotFound(
            this->response_str,
            request,
            "",
            _file_path,
            this->config);
    }

    // ---------- permissão ----------
    if (!isReadable(_file_path))
    {
        return StatusCodes::http403Forbidden(this->response_str, _file_path);
    }

    // ---------- arquivo estático ----------
    std::string content = readFile(_file_path);

    StatusCodes::http200FileFound(
        this->response_str,
        request,
        content,
        _file_path);
}

void Response::methodPost(const HttpRequest &request)
{
    // 1. Validar client_max_body_size (server e location)
    size_t body_size = request.getBody().size();

    // Validar limite global do servidor
    if (this->config.client_max_body_size > 0 && body_size > this->config.client_max_body_size)
    {
        std::cout << "[413] Body size (" << body_size << ") excede limite do servidor ("
                  << this->config.client_max_body_size << ")" << std::endl;
        return StatusCodes::http413PayloadTooLarge(this->response_str);
    }

    // Validar limite específico da location (mais restritivo)
    const LocationConfig *location = findMatchingLocation(request.getUri());
   
    if (location && location->client_max_body_size > 0 && body_size > location->client_max_body_size)
    {
        std::cout << "[413] Body size (" << body_size << ") excede limite da location ("
                  << location->client_max_body_size << ")" << std::endl;
        return StatusCodes::http413PayloadTooLarge(this->response_str);
    }

    // 2. Obter Content-Type
    std::string content_type = request.getHeader("Content-Type");
    if (content_type.empty())
    {
        // RFC 7231: 400 Bad Request para header obrigatório ausente
        std::cout << "[400] Content-Type header é obrigatório" << std::endl;
        return StatusCodes::http400BadRequest(this->response_str, "Content-Type header is required");
    }

    // 3. Processar conforme Content-Type

    // 3.1 multipart/form-data - Upload de arquivos
    if (content_type.find("multipart/form-data") != std::string::npos)
        return multipartFormData(request, content_type);

    // 3.2 application/x-www-form-urlencoded - Dados de formulário
    else if (content_type.find("application/x-www-form-urlencoded") != std::string::npos)
    {
        // Decodificar form data
        std::string decoded_body = urlDecode(request.getBody());

        // RFC 7231: 200 OK para processamento sem criar recurso
        std::ostringstream json_response;
        json_response << "{\"message\":\"Form data recebido\",";
        json_response << "\"size\":" << decoded_body.size() << "}";

        return StatusCodes::http200Ok(this->response_str, json_response.str());
    }

    // 3.3 application/json - Dados JSON
    else if (content_type.find("application/json") != std::string::npos)
    {
        // Processar JSON (validação básica)
        std::string json_body = request.getBody();

        std::ostringstream json_response;
        json_response << "{\"message\":\"JSON recebido\",";
        json_response << "\"size\":" << json_body.size() << "}";

        return StatusCodes::http200Ok(this->response_str, json_response.str());
    }

    // 3.4 text/plain - Texto simples
    else if (content_type.find("text/plain") != std::string::npos)
    {
        std::ostringstream json_response;
        json_response << "{\"message\":\"Text data recebido\",";
        json_response << "\"size\":" << request.getBody().size() << "}";

        return StatusCodes::http200Ok(this->response_str, json_response.str());
    }

    // Content-Type não suportado
    else
    {
        std::cout << "[415] Content-Type não suportado: " << content_type << std::endl;
        return StatusCodes::http415UnsupportedMediaType(this->response_str);
    }
}

void Response::methodDelete(const HttpRequest &request, const std::string &file_path)
{
    // 1. Verificar se arquivo existe
    if (!fileExists(file_path))
    {
        std::cout << "[404] Arquivo não encontrado para deletar: " << file_path << std::endl;
        return StatusCodes::http404NotFound(this->response_str, request, "", file_path, this->config);
    }

    // 2. Verificar se é diretório (não permitir deletar diretórios)
    if (isDirectory(file_path))
    {
        std::cout << "[403] Não é permitido deletar diretórios: " << file_path << std::endl;
        return StatusCodes::http403Forbidden(this->response_str, file_path);
    }

    // 3. Validar symlinks - resolver path real e verificar segurança
    std::string real_path = getRealPath(file_path);
    if (real_path.empty())
    {
        std::cout << "[403] Não foi possível resolver path real: " << file_path << std::endl;
        return StatusCodes::http403Forbidden(this->response_str, file_path);
    }

    // Validar que o path real ainda está dentro do root permitido
    std::string root = this->config.root;
    const LocationConfig *location = findMatchingLocation(request.getUri());
    if (location && !location->root.empty())
        root = location->root;

    if (!isPathSafe(real_path, root))
    {
        std::cout << "[403] Symlink aponta para fora do root permitido: " << file_path << " -> " << real_path << std::endl;
        return StatusCodes::http403Forbidden(this->response_str, file_path);
    }

    // 4. Verificar permissões de escrita no diretório pai
    std::string parent_dir = getParentDirectory(file_path);
    if (!hasWritePermission(parent_dir))
    {
        std::cout << "[403] Sem permissão para deletar: " << file_path << std::endl;
        return StatusCodes::http403Forbidden(this->response_str, file_path);
    }

    // 5. Verificar se o arquivo é protegido (ex: index.html)
    std::string filename = getFileName(file_path);
    if (isProtectedFile(filename))
    {
        std::cout << "[403] Arquivo protegido, não pode ser deletado: " << file_path << std::endl;
        return StatusCodes::http403Forbidden(this->response_str, file_path);
    }

    // Log antes de deletar
    size_t file_size = getFileSize(file_path);
    std::cout << "[DELETE] Arquivo: " << file_path << std::endl;
    std::cout << "[DELETE] Tamanho: " << file_size << " bytes" << std::endl;
    std::cout << "[DELETE] URI: " << request.getUri() << std::endl;

    // 6. Tentar deletar arquivo
    if (remove(file_path.c_str()) != 0)
    {
        std::cout << "[500] Erro ao deletar arquivo: " << strerror(errno) << std::endl;
        return StatusCodes::http500InternalServerError(this->response_str, "Failed to delete file: " + std::string(strerror(errno)));
    }

    // 6. Sucesso
    std::cout << "[DELETE] ✓ Arquivo deletado com sucesso" << std::endl;
    return StatusCodes::http204NoContent(this->response_str);
}



void Response::multipartFormData(const HttpRequest &request, const std::string &content_type)
{
    std::string boundary = extractBoundary(content_type);
    if (boundary.empty())
    {
        std::cout << "[400] Boundary não encontrado no Content-Type" << std::endl;
        return StatusCodes::http415UnsupportedMediaType(this->response_str);
    }

    std::vector<MultipartFile> files;
    if (!parseMultipartData(request.getBody(), boundary, files))
    {
        std::cout << "[400] Erro ao parsear multipart data" << std::endl;
        return StatusCodes::http415UnsupportedMediaType(this->response_str);
    }

    // Determinar diretório de upload. se estiver vazio significa que o body é muito grande
    std::string upload_dir = getUploadDir(request);
    if (upload_dir.empty())
        return StatusCodes::http413PayloadTooLarge(this->response_str);

    // Criar diretório se não existir
    if (!createDirectory(upload_dir))
    {
        std::cout << "[500] Erro ao criar diretório de upload: " << upload_dir << std::endl;
        return StatusCodes::http500InternalServerError(this->response_str, "Failed to create upload directory: " + upload_dir);
    }

    // Verificar permissões de escrita
    if (!hasWritePermission(upload_dir))
    {
        std::cout << "[403] Sem permissão de escrita em: " << upload_dir << std::endl;
        return StatusCodes::http403Forbidden(this->response_str, upload_dir);
    }

    // Salvar cada arquivo com validação
    // std::ostringstream saveFiles(files, upload_dir);
    std::ostringstream json_response;
    json_response << "{\"files\":[";
    std::vector<std::string> saved_files; // Para limpeza em caso de erro
    size_t success_count = 0;

    for (size_t i = 0; i < files.size(); ++i)
    {
        // Validar extensão do arquivo
        if (!isAllowedFileExtension(files[i].filename, this->allowed_extensions))
        {
            std::cout << "[400] Extensão de arquivo não permitida: " << files[i].filename << std::endl;
            cleanupFiles(saved_files); // Limpar arquivos já salvos
            return StatusCodes::http400BadRequest(this->response_str, "File extension not allowed: " + getFileExtension(files[i].filename));
        }

        // Validar tamanho individual do arquivo (max 10MB por arquivo)
        if (files[i].content.size() > 10 * 1024 * 1024)
        {
            std::cout << "[413] Arquivo muito grande: " << files[i].filename
                      << " (" << files[i].content.size() << " bytes)" << std::endl;
            cleanupFiles(saved_files);
            return StatusCodes::http413PayloadTooLarge(this->response_str);
        }

        // RFC 2388: Servidor pode renomear arquivo por segurança/conflitos
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
            return StatusCodes::http400BadRequest(this->response_str, "Failed to save file: " + files[i].filename);
        }
    }

    json_response << "],\"success\":true,\"count\":" << success_count << "}";

    // RFC 7231: Location header deve conter URI do recurso criado (não caminho filesystem)
    std::string location = "";
    if (!saved_files.empty())
    {
        // Extrair URI relativo: /tmp/uploads/arquivo.pdf -> /uploads/arquivo.pdf
        std::string first_file = saved_files[0];
        size_t upload_pos = first_file.find("/uploads/");
        if (upload_pos != std::string::npos)
            location = first_file.substr(upload_pos); // URI relativo ao servidor
        else
            location = request.getUri() + "/" + getFileName(first_file);
    }

    StatusCodes::http201Created(this->response_str, location, json_response.str());
}

const LocationConfig *Response::findMatchingLocation(const std::string &uri) const
{
    // Buscar a location que melhor corresponde ao URI (longest match first)
    const LocationConfig *best_match = NULL;
    size_t best_match_length = 0;

    for (size_t i = 0; i < this->config.locations.size(); i++)
    {
        const std::string &location_path = this->config.locations[i].path;

        // Verificar se URI começa com o path da location
        if (uri.find(location_path) == 0)
        {
            // Preferir match mais longo (mais específico)
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
    // Buscar location correspondente
    const LocationConfig *location = findMatchingLocation(request.getUri());

    // Se a location tem uma lista de métodos permitidos, verificar se o método da requisição está nela
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
    // Buscar location correspondente
    const LocationConfig *location = findMatchingLocation(request.getUri());

    // Verificar client_max_body_size específico da location
    if (location && location->client_max_body_size > 0 &&
        request.getBody().size() > location->client_max_body_size)
    {
        std::cout << "[413] Body size (" << request.getBody().size()
                  << ") excede limite da location ("
                  << location->client_max_body_size << ")" << std::endl;
        return "";
    }

    // Determinar upload_dir
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
    // 1. Abrir diretório
    DIR *dir = opendir(dir_path.c_str());

    // 2. Ler todos os arquivos
    std::vector<std::string> files;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        files.push_back(entry->d_name);
    }
    closedir(dir);

    // 3. Gerar HTML bonito
    std::ostringstream html;
    html << "<html><head><title>Index of " << uri << "</title></head>";
    html << "<body><h1>Index of " << uri << "</h1><hr><ul>";

    for (size_t i = 0; i < files.size(); i++)
    {
        if (files[i] != "." && files[i] != "..")
        {
            html << "<li><a href='" << uri << "/" << files[i] << "'>";
            html << files[i] << "</a></li>";
        }
    }

    html << "</ul><hr></body></html>";

    // 4. Retornar 200 OK com HTML
    StatusCodes::http200FileFound(this->response_str, request, html.str(), dir_path);
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
    // Comportamento alias: remove prefixo do location do URI
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
