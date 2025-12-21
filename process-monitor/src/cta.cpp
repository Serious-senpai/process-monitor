#include <nlohmann/json.hpp>

#include "config.hpp"
#include "net.hpp"
#include "utils.hpp"

using json = nlohmann::json;

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

    return procmon::cta_loop(port.value());
}
