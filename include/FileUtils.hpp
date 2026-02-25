/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   FileUtils.hpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nmatondo <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/11/03 15:40:22 by nmatondo          #+#    #+#             */
/*   Updated: 2026/02/25 11:25:41 by nmatondo         ###   ########.fr       */
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

# define ERROR_OPENING_FILE "Error: Could not open file"

std::string readFile(const std::string& filepath);
std::string create_error_message(const std::string& error);
void openFile(std::ifstream& file, const std::string& filename, std::string sms);

uint32_t ipToHex(const std::string& ip);

#endif
