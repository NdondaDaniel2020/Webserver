/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   FileUtils.hpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nmatondo <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/11/03 15:40:22 by nmatondo          #+#    #+#             */
/*   Updated: 2025/11/04 12:57:18 by nmatondo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */


#ifndef FILEUTILS_HPP
# define FILEUTILS_HPP
# include <iostream>
# include <sstream>
# include <fstream>
# include <cstring>
# include <errno.h>
# include <string>

std::string create_error_message(const std::string& error);
std::string readFile(const std::string& filepath);

#endif
