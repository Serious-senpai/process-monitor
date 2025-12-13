#include "net.hpp"

#include "config.hpp"
#include "utils.hpp"

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        return show_help();
    }

    auto port = parse_port(argv[1]);
    if (!port.has_value())
    {
        return show_help();
    }

    Threshold sample = {0};
    std::vector<procmon::ConfigEntry> entries;
    entries.push_back(procmon::ConfigEntry{
        .name = "curl.exe",
        .threshold = sample,
    });
    entries.push_back(procmon::ConfigEntry{
        .name = "notepad.exe",
        .threshold = sample,
    });

    auto listener = net::TcpListener::bind(net::SocketAddrV4(net::Ipv4Addr::LOCALHOST, port.value()));
    auto client = listener.unwrap().accept();
    if (client.is_ok())
    {
        auto stream = std::move(client).into_ok().first;

        uint8_t size = static_cast<uint8_t>(entries.size());
        stream.write(std::span<char>(reinterpret_cast<char *>(&size), sizeof(size))).unwrap();
        stream.write(std::span<char>(reinterpret_cast<char *>(entries.data()), sizeof(procmon::ConfigEntry) * entries.size())).unwrap();
        stream.flush();
    }

    return 0;
}
