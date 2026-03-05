#ifndef CGIHANDLER_HPP
# define CGIHANDLER_HPP

# include <string>
# include "HttpRequest.hpp"
# include "ConfigParser.hpp"

namespace CGIHandler {
    // Execute the CGI program located at scriptPath using the interpreter
    // specified in location.cgi_path. The resulting HTTP response (including
    // status line, headers and body) is written to outResponse. Returns true
    // on success, false on error.
    bool execute(const HttpRequest& request,
                 const std::string& scriptPath,
                 const LocationConfig& location,
                 std::string& outResponse);
}

#endif