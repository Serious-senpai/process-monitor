#pragma once

#include "config.hpp"
#include "net.hpp"

namespace procmon
{
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

    inline void trim_command_name(const char *src, StaticCommandName *dest)
    {
        std::memset(dest, 0, sizeof(StaticCommandName));
        for (size_t i = 0; i + 1 < COMMAND_LENGTH; i++)
        {
            if (src[i] == '\0')
            {
                break;
            }

            (*dest)[i] = src[i];
        }
    }

    inline io::Result<std::vector<char>> read_message(net::TcpStream &stream)
    {
        uint32_t length = 0;
        auto size = SHORT_CIRCUIT(std::vector<char>, stream.read(std::span<char>(reinterpret_cast<char *>(&length), sizeof(length))));

        std::vector<char> buffer(length);
        char *ptr = buffer.data();
        while (length > 0)
        {
            size = SHORT_CIRCUIT(std::vector<char>, stream.read(std::span<char>(ptr, length)));
            ptr += size;
            length -= size;
        }

        return io::Result<std::vector<char>>::ok(std::move(buffer));
    }

    struct ViolationInfo
    {
        uint32_t pid;
        StaticCommandName name;
        Violation violation;

        explicit ViolationInfo(uint32_t pid, const StaticCommandName &_name, Violation &&violation)
            : pid(pid), violation(std::move(violation))
        {
            std::memcpy(name, _name, sizeof(StaticCommandName));
        }
    };

    int cta_loop(uint16_t port);
    int ctb_loop(net::TcpListener &listener, const std::string &json_config);
}
