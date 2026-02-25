/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   FileUtils.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nmatondo <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/11/03 15:44:58 by nmatondo          #+#    #+#             */
/*   Updated: 2026/02/25 11:24:27 by nmatondo         ###   ########.fr       */
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
