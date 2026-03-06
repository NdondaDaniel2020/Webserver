/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   CGIHandler.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ajacinto <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/05 12:18:08 by ajacinto          #+#    #+#             */
/*   Updated: 2026/03/05 12:18:15 by ajacinto         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CGIHANDLER_HPP
# define CGIHANDLER_HPP

# include <string>
# include "HttpRequest.hpp"
# include "ConfigParser.hpp"

namespace CGIHandler {
    bool executeCgi(const HttpRequest& request,
                 const std::string& scriptPath,
                 const LocationConfig& location,
                 std::string& outResponse);
}

#endif
