#ifndef STRING_UTILS_HPP
# define STRING_UTILS_HPP

# include <string>
# include <cstdlib>

class StringUtils {
    public:
        static std::string trim(const std::string& str);
        static size_t parseSize(std::string size_str);
};

#endif
