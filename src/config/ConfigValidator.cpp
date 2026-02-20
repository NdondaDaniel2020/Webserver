# include "../include/configValidator.hpp"

 bool validatePort(std::string port)
 {
    int port_num;

    if (port.empty())
        return false;
    for (size_t i = 0; i < port.size(); ++i)
    {
        if (!isdigit(port[i]))
            return false;
    }
    port_num = atoi(port.c_str());
    return (port_num > 0 && port_num <= 65535);
 }