#pragma once

#include "io.hpp"
#include "generated/types.hpp"

namespace procmon
{
    struct ConfigEntry
    {
        StaticCommandName name;
        Threshold threshold;
    };

    io::Result<std::vector<ConfigEntry>> load_config();
    io::Result<std::monostate> save_config(const std::vector<ConfigEntry> &entries);
}
