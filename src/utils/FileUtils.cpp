/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   FileUtils.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nmatondo <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/11/03 15:44:58 by nmatondo          #+#    #+#             */
/*   Updated: 2025/11/03 15:46:01 by nmatondo         ###   ########.fr       */
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