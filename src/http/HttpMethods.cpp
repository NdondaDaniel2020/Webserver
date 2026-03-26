# include "Response.hpp"
# include "ResponseHelpers.hpp"
# include "FileUtils.hpp"
# include "StringUtils.hpp"
# include "StatusCodes.hpp"
# include "HttpRequest.hpp"
# include <iostream>
# include <sstream>
# include <cerrno>
# include <cstring>

void Response::methodGet(const HttpRequest &request, const std::string &file_path)
{
    std::string _file_path = file_path;

    const LocationConfig *location = findMatchingLocation(this->config, request.getUri());
    if (location && !location->cgi_handlers.empty())
        return StatusCodes::http502BadGateway(this->response_str, request, "Fail CGI", this->config);

    if (location && location->redirect_code > 0)
        return handleRedirect(this->response_str, location->redirect_code, location->redirect_url);
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

    const LocationConfig *location = findMatchingLocation(this->config, request.getUri());
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
    const LocationConfig *location = findMatchingLocation(this->config, request.getUri());
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
    if (isProtectedFile(this->protected_files, filename))
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