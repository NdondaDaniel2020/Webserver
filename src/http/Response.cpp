/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Response.cpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nmatondo <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/03 10:05:33 by nmatondo          #+#    #+#             */
/*   Updated: 2026/02/27 09:11:14 by nmatondo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Response.hpp"

Response::Response(const HttpRequest& request, const ServerConfig& config) : config(config)
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
    }
    return *this;
}


std::string Response::getResponseHttp()
{
    return this->response_str;
}


void Response::buildHttpResponse(const HttpRequest& request)
{
    // 1. Sanitizar URI para prevenir path traversal
    std::string uri = sanitizePath(request.getUri());
    std::string file_path = this->config.root + uri;

    // 2. Validar segurança - verificar se o caminho está dentro do root
    if (!isPathSafe(file_path, this->config.root))
    {
        std::cout << "[403] Path traversal bloqueado: " << request.getUri() << std::endl;
        return httpForbidden403(file_path);
    }

    // 3. Validar se método é permitido por location
    if (!validateAllowedMethod(request))
    {
        std::cout << "[405] Método " << request.getMethod() << " não permitido para: " << request.getUri() << std::endl;
        return methodNotAllowed405(file_path);
    }
    
    // 4. Executar método
    if (request.getMethod() == "GET")
        methodGet(request, file_path);
    else if (request.getMethod() == "POST")
        methodPost(request, file_path);
    else if (request.getMethod() == "DELETE")
        methodDelete(request, file_path);
    else
        methodNotAllowed405(file_path);
}

void Response::methodGet(const HttpRequest& request, const std::string& file_path)
{
    std::string _file_path = file_path;
    // 3. Verificar se é diretório
    if (isDirectory(_file_path))
    {
        // Buscar arquivo index configurado (index.html, etc)
        std::string index_path = findIndexFile(_file_path, this->config.index_files);
        
        if (!index_path.empty())
        {
            _file_path = index_path;
        }
        else
        {
            // TODO: Implementar autoindex se config permitir
            std::cout << "[403] Diretório sem arquivo index: " << _file_path << std::endl;
            return httpForbidden403(_file_path);
        }
    }
    
    // 4. Verificar se arquivo existe
    if (!fileExists(_file_path))
    {
        return httpFileNotFound404(request, "", _file_path);
    }
    
    // 5. Verificar permissões de leitura
    if (!isReadable(_file_path))
    {
        std::cout << "[403] Sem permissão de leitura: " << _file_path << std::endl;
        return httpForbidden403(_file_path);
    }
    
    // 6. Ler arquivo e retornar
    std::string content = readFile(_file_path);
    httpFileFound200(request, content, _file_path);
}

void Response::methodPost(const HttpRequest& request, const std::string& file_path)
{   
    // 1. Validar client_max_body_size
    size_t body_size = request.getBody().size();
    if (this->config.client_max_body_size > 0 && body_size > this->config.client_max_body_size)
    {
        std::cout << "[413] Body size (" << body_size << ") excede limite (" 
                  << this->config.client_max_body_size << ")" << std::endl;
        return httpPayloadTooLarge413();
    }
    
    // 2. Obter Content-Type
    std::string content_type = request.getHeader("Content-Type");
    if (content_type.empty())
    {
        std::cout << "[415] Content-Type não especificado" << std::endl;
        return httpUnsupportedMediaType415();
    }
    
    // 3. Processar conforme Content-Type

    // 3.1 multipart/form-data - Upload de arquivos
    if (content_type.find("multipart/form-data") != std::string::npos)
        return multipartFormData(request, file_path, content_type);

    // 3.2 application/x-www-form-urlencoded - Dados de formulário
    else if (content_type.find("application/x-www-form-urlencoded") != std::string::npos)
    {
        // Decodificar form data
        std::string decoded_body = urlDecode(request.getBody());
        
        // Aqui você pode processar os dados do formulário
        // Por exemplo, salvar em um arquivo ou processar conforme necessário
        
        std::ostringstream json_response;
        json_response << "{\"message\":\"Form data recebido\",";
        json_response << "\"size\":" << decoded_body.size() << "}";
        
        return httpCreated201("/form", json_response.str());
    }
    
    // 3.3 application/json - Dados JSON
    else if (content_type.find("application/json") != std::string::npos)
    {
        // Processar JSON (validação básica)
        std::string json_body = request.getBody();
        
        std::ostringstream json_response;
        json_response << "{\"message\":\"JSON recebido\",";
        json_response << "\"size\":" << json_body.size() << "}";
        
        return httpCreated201("/api", json_response.str());
    }
    
    // 3.4 text/plain - Texto simples
    else if (content_type.find("text/plain") != std::string::npos)
    {
        std::ostringstream json_response;
        json_response << "{\"message\":\"Text data recebido\",";
        json_response << "\"size\":" << request.getBody().size() << "}";
        
        return httpCreated201("/text", json_response.str());
    }
    
    // Content-Type não suportado
    else
    {
        std::cout << "[415] Content-Type não suportado: " << content_type << std::endl;
        return httpUnsupportedMediaType415();
    }
}

void Response::methodDelete(const HttpRequest& request, const std::string& file_path)
{
    (void)request;
    (void)file_path;
}




 
void Response::multipartFormData(const HttpRequest& request, const std::string& file_path, const std::string& content_type)
{
    std::string boundary = extractBoundary(content_type);
    if (boundary.empty())
    {
        std::cout << "[400] Boundary não encontrado no Content-Type" << std::endl;
        return httpUnsupportedMediaType415();
    }
    
    std::vector<MultipartFile> files;
    if (!parseMultipartData(request.getBody(), boundary, files))
    {
        std::cout << "[400] Erro ao parsear multipart data" << std::endl;
        return httpUnsupportedMediaType415();
    }
    
    // Determinar diretório de upload. se estiver vazio significa que o body é muito grande
    std::string upload_dir = getUploadDir(request, file_path);
    if (upload_dir.empty())
        return httpPayloadTooLarge413();
    
    // Criar diretório se não existir
    if (!createDirectory(upload_dir))
    {
        std::cout << "[500] Erro ao criar diretório de upload: " << upload_dir << std::endl;
        return httpForbidden403(upload_dir);
    }
    
    // Verificar permissões de escrita
    if (!hasWritePermission(upload_dir))
    {
        std::cout << "[403] Sem permissão de escrita em: " << upload_dir << std::endl;
        return httpForbidden403(upload_dir);
    }
      
    // Salvar cada arquivo com validação
    // std::ostringstream saveFiles(files, upload_dir);
    std::ostringstream json_response;
    json_response << "{\"files\":[";
    std::vector<std::string> saved_files;  // Para limpeza em caso de erro
    size_t success_count = 0;
    
    for (size_t i = 0; i < files.size(); ++i)
    {
        // Validar extensão do arquivo
        if (!isAllowedFileExtension(files[i].filename, this->allowed_extensions))
        {
            std::cout << "[400] Extensão de arquivo não permitida: " << files[i].filename << std::endl;
            cleanupFiles(saved_files);  // Limpar arquivos já salvos
            return httpBadRequest400("File extension not allowed: " + getFileExtension(files[i].filename));
        }
        
        // Validar tamanho individual do arquivo (max 10MB por arquivo)
        if (files[i].content.size() > 10 * 1024 * 1024)
        {
            std::cout << "[413] Arquivo muito grande: " << files[i].filename 
                      << " (" << files[i].content.size() << " bytes)" << std::endl;
            cleanupFiles(saved_files);
            return httpPayloadTooLarge413();
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
            cleanupFiles(saved_files);  // Limpar todos em caso de erro
            return httpBadRequest400("Failed to save file: " + files[i].filename);
        }
    }
    
    json_response << "],\"success\":true,\"count\":" << success_count << "}";
    
    // Retornar 201 Created com Location do primeiro arquivo
    std::string location = "";
    if (!saved_files.empty())
        location = saved_files[0];
    
    httpCreated201(location, json_response.str());
}





void Response::httpFileNotFound404(const HttpRequest& request, const std::string& content, const std::string& file_path)
{
    // Arquivo não encontrado - retornar 404
    std::cout << "[404] Arquivo não encontrado: " << file_path << std::endl;
    
    // Buscar página de erro 404 personalizada
    std::string error_page_404;
    std::map<std::string, std::string>::const_iterator it = this->config.error_pages.find("404");
    if (it != this->config.error_pages.end())
        error_page_404 = this->config.root + it->second;
    
    std::string final_content = content;
    if (!error_page_404.empty())
        final_content = readFile(error_page_404);
    
    if (final_content.empty())
        final_content = "<html><body><h1>404 Not Found</h1></body></html>";
    
    std::ostringstream oss;
    oss << "HTTP/1.1 404 Not Found\r\n";
    oss << "Date: " << getCurrentHttpDate() << "\r\n";
    oss << "Server: webserv/1.0\r\n";
    oss << "Content-Type: text/html; charset=UTF-8\r\n";
    oss << "Content-Length: " << final_content.size() << "\r\n";
    
    if (request.getHeader("Connection") != "" && request.getHeader("Connection") == "keep-alive")
        oss << "Connection: keep-alive\r\n";
    else
        oss << "Connection: close\r\n";

    oss << "\r\n";
    oss << final_content;
    this->response_str = oss.str();
}

void Response::httpFileFound200(const HttpRequest& request, const std::string& content, const std::string& file_path)
{
    // Arquivo encontrado - retornar 200 OK
    std::cout << "[200] Arquivo encontrado: " << file_path << " (" << content.size() << " bytes)" << std::endl;

    // Detectar MIME type correto baseado na extensão
    std::string mime_type = getMimeType(file_path);

    std::ostringstream oss;
    oss << "HTTP/1.1 200 OK\r\n";
    oss << "Date: " << getCurrentHttpDate() << "\r\n";
    oss << "Server: webserv/1.0\r\n";
    oss << "Content-Type: " << mime_type << "\r\n";
    oss << "Content-Length: " << content.size() << "\r\n";
    oss << "Last-Modified: " << getFileModifiedDate(file_path) << "\r\n";

    if (request.getHeader("Connection") != "" && request.getHeader("Connection") == "keep-alive")
        oss << "Connection: keep-alive\r\n";
    else
        oss << "Connection: close\r\n";

    oss << "\r\n";
    oss << content;
    this->response_str = oss.str();
}

void Response::methodNotAllowed405(const std::string& file_path)
{
    // Método não permitido - retornar 405 Method Not Allowed
    std::cout << "[405] Método não permitido para: " << file_path << std::endl;

    std::ostringstream oss;
    oss << "HTTP/1.1 405 Method Not Allowed\r\n";
    oss << "Date: " << getCurrentHttpDate() << "\r\n";
    oss << "Server: webserv/1.0\r\n";
    oss << "Content-Type: text/html; charset=UTF-8\r\n";
    oss << "Content-Length: 0\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    this->response_str = oss.str();
}

void Response::httpForbidden403(const std::string& file_path)
{
    // Acesso proibido - retornar 403 Forbidden
    std::cout << "[403] Acesso proibido: " << file_path << std::endl;
    
    std::string content = "<html><body><h1>403 Forbidden</h1><p>Access Denied</p></body></html>";
    
    std::ostringstream oss;
    oss << "HTTP/1.1 403 Forbidden\r\n";
    oss << "Date: " << getCurrentHttpDate() << "\r\n";
    oss << "Server: webserv/1.0\r\n";
    oss << "Content-Type: text/html; charset=UTF-8\r\n";
    oss << "Content-Length: " << content.size() << "\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << content;
    this->response_str = oss.str();
}

void Response::httpCreated201(const std::string& location, const std::string& message)
{
    // Recurso criado - retornar 201 Created
    std::cout << "[201] Recurso criado: " << location << std::endl;
    
    std::ostringstream oss;
    oss << "HTTP/1.1 201 Created\r\n";
    oss << "Date: " << getCurrentHttpDate() << "\r\n";
    oss << "Server: webserv/1.0\r\n";
    oss << "Location: " << location << "\r\n";
    oss << "Content-Type: application/json; charset=UTF-8\r\n";
    oss << "Content-Length: " << message.size() << "\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << message;
    this->response_str = oss.str();
}

void Response::httpPayloadTooLarge413()
{
    // Body muito grande - retornar 413 Payload Too Large
    std::cout << "[413] Payload Too Large" << std::endl;
    
    std::string content = "<html><body><h1>413 Payload Too Large</h1></body></html>";
    
    std::ostringstream oss;
    oss << "HTTP/1.1 413 Payload Too Large\r\n";
    oss << "Date: " << getCurrentHttpDate() << "\r\n";
    oss << "Server: webserv/1.0\r\n";
    oss << "Content-Type: text/html; charset=UTF-8\r\n";
    oss << "Content-Length: " << content.size() << "\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << content;
    this->response_str = oss.str();
}

void Response::httpUnsupportedMediaType415()
{
    // Content-Type não suportado - retornar 415 Unsupported Media Type
    std::cout << "[415] Unsupported Media Type" << std::endl;
    
    std::string content = "<html><body><h1>415 Unsupported Media Type</h1></body></html>";
    
    std::ostringstream oss;
    oss << "HTTP/1.1 415 Unsupported Media Type\r\n";
    oss << "Date: " << getCurrentHttpDate() << "\r\n";
    oss << "Server: webserv/1.0\r\n";
    oss << "Content-Type: text/html; charset=UTF-8\r\n";
    oss << "Content-Length: " << content.size() << "\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << content;
    this->response_str = oss.str();
}

void Response::httpBadRequest400(const std::string& message)
{
    // Requisição malformada - retornar 400 Bad Request
    std::cout << "[400] Bad Request: " << message << std::endl;
    
    std::ostringstream json_response;
    json_response << "{\"error\":\"Bad Request\",\"message\":\"" << message << "\"}";
    std::string content = json_response.str();
    
    std::ostringstream oss;
    oss << "HTTP/1.1 400 Bad Request\r\n";
    oss << "Date: " << getCurrentHttpDate() << "\r\n";
    oss << "Server: webserv/1.0\r\n";
    oss << "Content-Type: application/json; charset=UTF-8\r\n";
    oss << "Content-Length: " << content.size() << "\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << content;
    this->response_str = oss.str();
}




bool Response::validateAllowedMethod(const HttpRequest& request)
{
    // Buscar location correspondente e validar método permitido
    const LocationConfig* matched_location = NULL;

    for (size_t i = 0; i < this->config.locations.size(); i++)
    {
        if (request.getUri().find(this->config.locations[i].path) == 0)
        {
            matched_location = &this->config.locations[i];
            break;
        }
    }
    
    // Se a location tem uma lista de métodos permitidos, verificar se o método da requisição está nela
    if (matched_location && !matched_location->allowed_methods.empty())
    {
        std::vector<std::string>::const_iterator it = std::find(
            matched_location->allowed_methods.begin(),
            matched_location->allowed_methods.end(),
            request.getMethod()
        );
        
        if (it == matched_location->allowed_methods.end())
            return false;
    }
    return true;
}

std::string Response::getUploadDir(const HttpRequest& request, const std::string& file_path)
{
    // Determinar diretório de upload (usar root se não houver upload_dir configurado)
    std::string upload_dir = this->config.root;
    for (size_t i = 0; i < this->config.locations.size(); i++)
    {
        if (file_path.find(this->config.locations[i].path) != std::string::npos)
        {
            // Verificar client_max_body_size específico da location
            if (this->config.locations[i].client_max_body_size > 0 && request.getBody().size() > this->config.locations[i].client_max_body_size)
            {
                std::cout << "[413] Body size (" << request.getBody().size() << ") excede limite da location (" 
                            << this->config.locations[i].client_max_body_size << ")" << std::endl;
                return "";
            }
            // Determinar upload_dir específico da location
            if (!this->config.locations[i].upload_dir.empty())
            {
                if (this->config.locations[i].upload_dir[0] != '/')
                    upload_dir = this->config.root + "/" + this->config.locations[i].upload_dir;
                else
                    upload_dir = this->config.locations[i].upload_dir;
            }
            break;
        }
    }
    return upload_dir;
}

std::ostringstream saveFiles(const std::vector<MultipartFile> files, const std::string& upload_dir)
{
    
}
