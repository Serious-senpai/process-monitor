#include "config.hpp"
#include "fs.hpp"
#include "net.hpp"
#include "utils.hpp"

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        return procmon::show_help();
    }

    auto port = procmon::parse_port(argv[1]);
    if (!port.has_value())
    {
        return procmon::show_help();
    }

    Threshold sample = {0};
    std::vector<procmon::ConfigEntry> entries;

    // TODO: Better path handling
    auto file_result = fs::File::open(path::PathBuf("monitor.json"));
    if (file_result.is_ok())
    {
        auto file = std::move(file_result).into_ok();

        // Read file contents
        std::vector<char> buffer(8192);
        std::string content;

        while (true)
        {
            auto read_result = file.read(std::span<char>(buffer.data(), buffer.size()));
            if (read_result.is_err())
            {
                break;
            }

            size_t bytes_read = std::move(read_result).into_ok();
            if (bytes_read == 0)
            {
                break;
            }

            content.append(buffer.data(), bytes_read);
        }

        auto listener = net::TcpListener::bind(net::SocketAddrV4(net::Ipv4Addr::LOCALHOST, port.value()));
        if (listener.is_ok())
        {
            return procmon::ctb_loop(listener.unwrap(), content);
        }
        else
        {
            std::cerr << "Failed to bind to port " << port.value() << ": " << listener.unwrap_err().message() << std::endl;
        }
    }
    else
    {
        std::cerr << "Unable to open JSON file: " << file_result.unwrap_err().message() << std::endl;
    }

    return 1;
}
