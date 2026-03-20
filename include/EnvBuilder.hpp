/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   EnvBuilder.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nmatondo <marvin@42.fr>                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/05 12:18:28 by ajacinto          #+#    #+#             */
/*   Updated: 2026/03/20 14:01:00 by nmatondo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef ENVBUILDER_HPP
# define ENVBUILDER_HPP

# include <string>
# include <vector>
# include "HttpRequest.hpp"
# include "ConfigParser.hpp"

namespace EnvBuilder {
    std::vector<std::string> build(const HttpRequest& request,
                                   const std::string& scriptPath);
}

#endif
