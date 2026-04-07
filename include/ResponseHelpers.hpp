/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ResponseHelpers.hpp                                :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nmatondo <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/26 10:00:00 by nmatondo          #+#    #+#             */
/*   Updated: 2026/04/07 08:07:37 by nmatondo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef RESPONSEHELPERS_HPP
# define RESPONSEHELPERS_HPP
# include "Response.hpp"

const LocationConfig *findMatchingLocation(const ServerConfig &config, const std::string &uri);
bool validateAllowedMethod(const ServerConfig &config, const HttpRequest &request);
std::string removeLocationInUri(const std::string &uri, const LocationConfig *location);
std::string getUploadDir(const ServerConfig &config, const HttpRequest &request);
void handleRedirect(std::string &response_str, int code, const std::string &url);

#endif
