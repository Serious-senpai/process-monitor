#include "config.hpp"
#include "pch.hpp"

constexpr LPCWSTR _REGISTRY_KEY = L"Software\\ProcessMonitor";

class _RegistryKeyHandler
{
public:
    HKEY key;

    explicit _RegistryKeyHandler(HKEY key) : key(key) {}

    ~_RegistryKeyHandler()
    {
        RegCloseKey(key);
    }
};

namespace config
{
    io::Result<std::vector<ConfigEntry>> load_config()
    {
        HKEY hkey;
        auto status = RegCreateKeyExW(
            HKEY_CURRENT_USER,
            _REGISTRY_KEY,
            0,
            NULL,
            REG_OPTION_NON_VOLATILE,
            KEY_ALL_ACCESS,
            NULL,
            &hkey,
            NULL);
        if (status != ERROR_SUCCESS)
        {
            return io::Result<std::vector<ConfigEntry>>::err(io::Error::from_raw_os_error(status));
        }

        _RegistryKeyHandler key(hkey);

        DWORD size = 0;
        status = RegQueryValueExW(hkey, NULL, NULL, NULL, NULL, &size);
        if (status != ERROR_SUCCESS)
        {
            return io::Result<std::vector<ConfigEntry>>::err(io::Error::from_raw_os_error(status));
        }

        auto unit_size = sizeof(ConfigEntry);
        if (size % unit_size != 0)
        {
            return io::Result<std::vector<ConfigEntry>>::err(io::Error::other(std::format("Unexpected size {}, which is not divisible by {}", size, unit_size)));
        }

        std::vector<ConfigEntry> result(size / unit_size);

        DWORD new_size = 0;
        status = RegQueryValueExW(hkey, NULL, NULL, NULL, reinterpret_cast<LPBYTE>(result.data()), &new_size);
        if (status != ERROR_SUCCESS)
        {
            return io::Result<std::vector<ConfigEntry>>::err(io::Error::from_raw_os_error(status));
        }

        if (size != new_size)
        {
            return io::Result<std::vector<ConfigEntry>>::err(io::Error::other(std::format("Registry size {} != {}. Most likely a race condition happened.", size, new_size)));
        }

        return io::Result<std::vector<ConfigEntry>>::ok(std::move(result));
    }

    io::Result<std::monostate> save_config(const std::vector<ConfigEntry> &entries)
    {
        return io::Result<std::monostate>::ok(std::monostate{});
    }
}
