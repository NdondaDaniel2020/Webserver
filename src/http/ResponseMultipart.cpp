#include "Response.hpp"
#include "ResponseHelpers.hpp"

void Response::multipartFormData(const HttpRequest &request, const std::string &content_type)
{
    std::string boundary = extractBoundary(content_type);
    if (boundary.empty())
        return StatusCodes::http415UnsupportedMediaType(this->response_str, request, this->config);

    std::vector<MultipartFile> files;
    if (!parseMultipartData(request.getBody(), boundary, files))
        return StatusCodes::http415UnsupportedMediaType(this->response_str, request, this->config);

    std::string upload_dir = getUploadDir(this->config, request);
    if (upload_dir.empty())
        return StatusCodes::http413PayloadTooLarge(this->response_str, this->config, request);

    if (!createDirectory(upload_dir))
        return StatusCodes::http500InternalServerError(this->response_str, request, "Failed to create upload directory: " + upload_dir, this->config);

    if (!hasWritePermission(upload_dir))
        return StatusCodes::http403Forbidden(this->response_str, request, upload_dir, this->config);

    std::ostringstream json_response;
    json_response << "{\"files\":[";
    std::vector<std::string> saved_files;
    size_t success_count = 0;
    const LocationConfig *location_config = findMatchingLocation(this->config, request.getUri());
    for (size_t i = 0; i < files.size(); ++i)
    {
        if (!isAllowedFileExtension(files[i].filename, this->allowed_extensions))
        {
            std::cout << "[400] Extensão de arquivo não permitida: " << files[i].filename << std::endl;
            cleanupFiles(saved_files); // Limpar arquivos já salvos
            return StatusCodes::http400BadRequest(this->response_str, "File extension not allowed: " + getFileExtension(files[i].filename), this->config, request);
        }

        if ((this->config.client_max_body_size > 0 && files[i].content.size() > this->config.client_max_body_size)
            || (location_config && location_config->client_max_body_size > 0 && files[i].content.size() > location_config->client_max_body_size))
        {
            std::cout << "[413] Arquivo muito grande: " << files[i].filename
                      << " (" << files[i].content.size() << " bytes)" << std::endl;
            cleanupFiles(saved_files);
            return StatusCodes::http413PayloadTooLarge(this->response_str, this->config, request);
        }

        // SANITIZAR filename para evitar path traversal
        std::string safe_filename = sanitizeFilename(files[i].filename);
        std::string unique_filename = generateUniqueFilename(safe_filename);
        std::string full_path = upload_dir + "/" + unique_filename;

        // Validação lógica de segurança: verificar tentativas de path traversal
        if (full_path.find("..") != std::string::npos || unique_filename.find("..") != std::string::npos)
        {
            std::cerr << "[403] Attempted path traversal: " << files[i].filename << std::endl;
            cleanupFiles(saved_files);
            return StatusCodes::http403Forbidden(this->response_str, request, "", this->config);
        }
        
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
            return StatusCodes::http400BadRequest(this->response_str, "Failed to save file: " + files[i].filename, this->config, request);
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

    StatusCodes::http201Created(this->response_str, location, json_response.str(), request, this->config);
}