#include "win32/path.hpp"

io::Result<std::wstring> maybe_verbatim(const char *s)
{
    auto wstr = SHORT_CIRCUIT(std::wstring, to_widestring(s));
    return get_long_path(std::move(wstr), true);
}

io::Result<std::wstring> to_widestring(const char *s)
{
    if (s == nullptr)
    {
        return io::Result<std::wstring>::ok(std::wstring());
    }

    int required = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
    if (required == 0)
    {
        return io::Result<std::wstring>::err(
            io::IoError(io::IoErrorKind::Os, std::format("MultiByteToWideChar: OS error {}", GetLastError())));
    }

    std::wstring result(required - 1, L'\0'); // exclude null terminator
    if (MultiByteToWideChar(CP_UTF8, 0, s, -1, result.data(), required) == 0)
    {
        return io::Result<std::wstring>::err(
            io::IoError(io::IoErrorKind::Os, std::format("MultiByteToWideChar: OS error {}", GetLastError())));
    }

    return io::Result<std::wstring>::ok(std::move(result));
}

io::Result<std::wstring> get_long_path(std::wstring &&path, bool prefer_verbatim)
{
    const size_t LEGACY_MAX_PATH = 248;
    const wchar_t SEP = L'\\';
    const wchar_t ALT_SEP = L'/';
    const wchar_t QUERY = L'?';
    const wchar_t COLON = L':';
    const wchar_t DOT = L'.';

    const std::wstring_view VERBATIM_PREFIX = L"\\\\?\\";
    const std::wstring_view NT_PREFIX = L"\\??\\";
    const std::wstring_view UNC_PREFIX = L"\\\\?\\UNC\\";

    // Early return for paths that are already verbatim or empty
    if (path.starts_with(VERBATIM_PREFIX) || path.starts_with(NT_PREFIX) || path == L"\0")
    {
        return io::Result<std::wstring>::ok(std::move(path));
    }
    else if (path.length() < LEGACY_MAX_PATH)
    {
        // Early return optimization for short absolute paths
        if (path.length() >= 2)
        {
            wchar_t first = path[0];
            wchar_t second = path[1];

            if (second == COLON && first != SEP && first != ALT_SEP)
            {
                if (path.length() == 2 || path[2] == SEP || path[2] == ALT_SEP)
                {
                    return io::Result<std::wstring>::ok(std::move(path));
                }
            }
            else if ((first == SEP || first == ALT_SEP) && (second == SEP || second == ALT_SEP))
            {
                return io::Result<std::wstring>::ok(std::move(path));
            }
        }
    }

    // Get absolute path using GetFullPathNameW
    DWORD required = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
    if (required == 0)
    {
        return io::Result<std::wstring>::err(
            io::IoError(io::IoErrorKind::Os, std::format("GetFullPathNameW: OS error {}", GetLastError())));
    }

    std::wstring absolute(required - 1, L'\0');
    if (GetFullPathNameW(path.c_str(), required, absolute.data(), nullptr) == 0)
    {
        return io::Result<std::wstring>::err(
            io::IoError(io::IoErrorKind::Os, std::format("GetFullPathNameW: OS error {}", GetLastError())));
    }

    std::wstring result;
    std::wstring_view absolute_view = absolute;

    // Only prepend prefix if needed
    if (prefer_verbatim || absolute.length() + 1 >= LEGACY_MAX_PATH)
    {
        std::wstring_view prefix;

        if (absolute.length() >= 3 && absolute[1] == COLON && absolute[2] == SEP)
        {
            // C:\ => \\?\C:\
                prefix = VERBATIM_PREFIX;
        }
        else if (absolute.length() >= 4 && absolute[0] == SEP && absolute[1] == SEP &&
                 absolute[2] == DOT && absolute[3] == SEP)
        {
            // \\.\ => \\?\
                absolute_view = absolute_view.substr(4);
            prefix = VERBATIM_PREFIX;
        }
        else if (absolute.length() >= 4 && absolute[0] == SEP &&
                 ((absolute[1] == SEP && absolute[2] == QUERY && absolute[3] == SEP) ||
                  (absolute[1] == QUERY && absolute[2] == QUERY && absolute[3] == SEP)))
        {
            // Leave \\?\ and \??\ as-is
            prefix = L"";
        }
        else if (absolute.length() >= 2 && absolute[0] == SEP && absolute[1] == SEP)
        {
            // \\ => \\?\UNC\
                absolute_view = absolute_view.substr(2);
            prefix = UNC_PREFIX;
        }

        result.reserve(prefix.length() + absolute_view.length());
        result.append(prefix);
    }
    else
    {
        result.reserve(absolute_view.length());
    }

    result.append(absolute_view);
    return io::Result<std::wstring>::ok(std::move(result));
}
