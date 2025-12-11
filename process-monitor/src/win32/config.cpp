#include "config.hpp"
#include "pch.hpp"

constexpr LPCWSTR _REGISTRY_KEY = L"Software\\ProcessMonitor";
constexpr LPCWSTR _REGISTRY_VALUE_NAME = L"Config";
constexpr DWORD _REGISTRY_SIZE_LIMIT = 8192;

class _RegistryKeyGuard
{
public:
    HKEY key;

    explicit _RegistryKeyGuard(HKEY key) : key(key) {}

    ~_RegistryKeyGuard()
    {
        RegCloseKey(key);
    }
};

namespace procmon
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
            KEY_READ,
            NULL,
            &hkey,
            NULL);
        if (status != ERROR_SUCCESS)
        {
            return io::Result<std::vector<ConfigEntry>>::err(io::Error::from_raw_os_error(status));
        }

        _RegistryKeyGuard key(hkey);

        DWORD size = 0;
        status = RegQueryValueExW(hkey, _REGISTRY_VALUE_NAME, NULL, NULL, NULL, &size);
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

        DWORD new_size = size;
        status = RegQueryValueExW(hkey, _REGISTRY_VALUE_NAME, NULL, NULL, reinterpret_cast<LPBYTE>(result.data()), &new_size);
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
        HKEY hkey;
        auto status = RegCreateKeyExW(
            HKEY_CURRENT_USER,
            _REGISTRY_KEY,
            0,
            NULL,
            REG_OPTION_NON_VOLATILE,
            KEY_WRITE,
            NULL,
            &hkey,
            NULL);
        if (status != ERROR_SUCCESS)
        {
            return io::Result<std::monostate>::err(io::Error::from_raw_os_error(status));
        }

        _RegistryKeyGuard key(hkey);

        constexpr DWORD max_entries = _REGISTRY_SIZE_LIMIT / sizeof(ConfigEntry);
        if (entries.size() > max_entries)
        {
            return io::Result<std::monostate>::err(io::Error::other("Config data too large for registry value"));
        }

        DWORD size = static_cast<DWORD>(entries.size() * sizeof(ConfigEntry));
        const BYTE *data_ptr = entries.empty() ? nullptr : reinterpret_cast<const BYTE *>(entries.data());

        status = RegSetValueExW(hkey, _REGISTRY_VALUE_NAME, 0, REG_BINARY, data_ptr, size);
        if (status != ERROR_SUCCESS)
        {
            return io::Result<std::monostate>::err(io::Error::from_raw_os_error(status));
        }

        return io::Result<std::monostate>::ok(std::monostate{});
    }
}
