#include "net.hpp"

#include "config.hpp"
#include "utils.hpp"

extern int cta_main(ConnectFunc connect);

uint16_t port;

void connect(
    bool *stopped,
    void (*sleep)(uint64_t milliseconds),
    void (*on_new_config)(const std::vector<procmon::ConfigEntry> &entries))
{
    while (!*stopped)
    {
        auto stream = net::TcpStream::connect(net::SocketAddrV4(net::Ipv4Addr::LOCALHOST, port));
        if (stream.is_ok())
        {
            uint8_t config_size = 0;
            auto size = stream.unwrap().read(std::span<char>(reinterpret_cast<char *>(&config_size), sizeof(config_size)));
            if (size.is_ok() && size.unwrap() == sizeof(config_size))
            {
                std::cerr << "config_size = " << config_size << std::endl;
                std::vector<procmon::ConfigEntry> entries(config_size);
                char *ptr = reinterpret_cast<char *>(entries.data());
                char *end = ptr + sizeof(procmon::ConfigEntry) * config_size;

                bool error = false;
                while (!*stopped && ptr != end)
                {
                    size = stream.unwrap().read(std::span<char>(ptr, static_cast<size_t>(end - ptr)));
                    if (size.is_err())
                    {
                        error = true;
                        break;
                    }

                    ptr += size.unwrap();
                    std::cerr << "Read " << size.unwrap() << " bytes" << std::endl;
                }

                if (!error)
                {
                    on_new_config(entries);
                    return;
                }
            }
        }
    }
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        return show_help();
    }

    auto oport = parse_port(argv[1]);
    if (!oport.has_value())
    {
        return show_help();
    }

    port = oport.value();
    return cta_main(connect);
}
