#pragma once

#include "config.hpp"

using ConnectFunc = void (*)(
    bool *stopped,
    void (*sleep)(uint64_t milliseconds),
    void (*on_new_config)(const std::vector<procmon::ConfigEntry> &entries));

inline std::optional<uint16_t> parse_port(char *argv)
{
    try
    {
        size_t pos = 0;
        unsigned long port = std::stoul(argv, &pos);
        if (pos != std::strlen(argv) || port > std::numeric_limits<uint16_t>::max())
        {
            return std::nullopt;
        }

        return port;
    }
    catch (...)
    {
        return std::nullopt;
    }
}

inline int show_help()
{
    std::cout << "Pass a port number as the only argument." << std::endl;
    return 1;
}
