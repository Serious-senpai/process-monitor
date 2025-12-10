#include <config.hpp>

int main()
{
    auto make_entry = [](const std::string &name, Threshold threshold)
    {
        config::ConfigEntry entry{};
        const size_t copy_len = std::min(name.size(), static_cast<size_t>(COMMAND_LENGTH));
        std::memcpy(entry.name, name.data(), copy_len);
        entry.threshold = threshold;
        return entry;
    };

    std::vector<config::ConfigEntry> entries{
        make_entry("ProcAlpha", Threshold{{25, 50, 75, 90}}),
        make_entry("ProcBeta", Threshold{{15, 35, 55, 80}}),
        make_entry("ProcGamma", Threshold{{10, 20, 30, 40}}),
    };

    auto save_result = config::save_config(entries);
    if (!save_result.is_ok())
    {
        std::cout << "save error: " << save_result.unwrap_err().message() << std::endl;
        return 1;
    }

    auto cfgentry = config::load_config();
    if (cfgentry.is_ok())
    {
        std::cout << "loaded entries: " << cfgentry.unwrap().size() << std::endl;
    }
    else
    {
        std::cout << "load error: " << cfgentry.unwrap_err().message() << std::endl;
    }
    return 0;
}
