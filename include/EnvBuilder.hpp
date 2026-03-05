#ifndef ENVBUILDER_HPP
# define ENVBUILDER_HPP

# include <string>
# include <vector>
# include "HttpRequest.hpp"
# include "ConfigParser.hpp"

namespace EnvBuilder {
    // Build a list of environment variable strings suitable for passing to
    // execve when running a CGI script. The returned vector owns the strings
    // (caller must keep it alive while using the C pointers).
    std::vector<std::string> build(const HttpRequest& request,
                                   const LocationConfig& location,
                                   const std::string& scriptPath);
}

#endif