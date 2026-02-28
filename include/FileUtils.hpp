/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   FileUtils.hpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nmatondo <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/11/03 15:40:22 by nmatondo          #+#    #+#             */
/*   Updated: 2026/02/28 09:22:23 by nmatondo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */


#ifndef FILEUTILS_HPP
# define FILEUTILS_HPP

# include <arpa/inet.h>
# include <iostream>
# include <sstream>
# include <fstream>
# include <cstdlib>
# include <cstring>
# include <errno.h>
# include <string>
# include <vector>
# include <sys/stat.h>
# include <unistd.h>
# include <limits.h>
# include <ctime>
# include "ConfigParser.hpp"

# define ERROR_OPENING_FILE "Error: Could not open file"

std::string readFile(const std::string& filepath);
std::string create_error_message(const std::string& error);
void openFile(std::ifstream& file, const std::string& filename, std::string sms);

uint32_t ipToHex(const std::string& ip);

// Path manipulation and security
std::string sanitizePath(const std::string& path);
std::string normalizePath(const std::string& path);
bool isPathSafe(const std::string& path, const std::string& root);
std::string getRealPath(const std::string& path);

// File system checks
bool fileExists(const std::string& path);
bool isDirectory(const std::string& path);
bool isReadable(const std::string& path);
std::string findIndexFile(const std::string& dir_path, const std::vector<std::string>& index_files);

// MIME types
std::string getMimeType(const std::string& path);

// HTTP date
std::string getCurrentHttpDate();
std::string getFileModifiedDate(const std::string& path);

// POST utilities
bool createDirectory(const std::string& path);
bool hasWritePermission(const std::string& path);
std::string generateUniqueFilename(const std::string& original_name);
bool writeFileToDisk(const std::string& filepath, const std::string& content);
std::string urlDecode(const std::string& str);
std::string extractBoundary(const std::string& content_type);

struct MultipartFile {
    std::string filename;
    std::string content_type;
    std::string content;
};

bool parseMultipartData(const std::string& body, const std::string& boundary, std::vector<MultipartFile>& files);

// File validation
bool isAllowedFileExtension(const std::string& filename, const std::vector<std::string>& allowed_extensions);
std::string getFileExtension(const std::string& filename);
void cleanupFiles(const std::vector<std::string>& file_paths);
std::string getParentDirectory(const std::string& path);
size_t getFileSize(const std::string& path);
std::string getFileName(const std::string& path);
std::string removeLocationInUri(const std::string& uri,  const LocationConfig* location);

#endif
