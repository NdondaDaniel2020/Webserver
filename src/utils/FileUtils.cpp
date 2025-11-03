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
