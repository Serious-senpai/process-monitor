#include "config.hpp"
#include "fs.hpp"

path::PathBuf _config_path()
{
    const char *home = std::getenv("HOME");
    if (home != nullptr && home[0] != '\0')
    {
        return path::PathBuf(home) / ".config" / "process-monitor.bin";
    }

    // Fall back to a relative path if HOME is unavailable.
    return path::PathBuf(".config") / "process-monitor.bin";
}

namespace procmon
{
    io::Result<std::vector<ConfigEntry>> load_config()
    {
        auto file = SHORT_CIRCUIT(std::vector<ConfigEntry>, fs::File::open(_config_path()));

        uint32_t size = 0;
        SHORT_CIRCUIT(std::vector<ConfigEntry>, file.read(std::span<char>(reinterpret_cast<char *>(&size), sizeof(size))));

        std::vector<ConfigEntry> result(size);
        for (uint32_t i = 0; i < size; i++)
        {
            SHORT_CIRCUIT(
                std::vector<ConfigEntry>,
                file.read(std::span<char>(reinterpret_cast<char *>(&result[i]), sizeof(ConfigEntry))));
        }

        return io::Result<std::vector<ConfigEntry>>::ok(std::move(result));
    }

    io::Result<std::monostate> save_config(const std::vector<ConfigEntry> &entries)
    {
        auto path = _config_path();
        auto dir = path.parent_path();
        if (!dir.empty())
        {
            SHORT_CIRCUIT(std::monostate, fs::create_dir_all(dir));
        }

        auto file = SHORT_CIRCUIT(std::monostate, fs::File::create(path));

        uint32_t size = entries.size();
        SHORT_CIRCUIT(
            std::monostate,
            file.write(std::span<const char>(reinterpret_cast<const char *>(&size), sizeof(size))));
        for (auto &entry : entries)
        {
            file.write(std::span<const char>(reinterpret_cast<const char *>(&entry), sizeof(ConfigEntry)));
        }
        return io::Result<std::monostate>::ok(std::monostate{});
    }
}
