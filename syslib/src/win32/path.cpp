#include "path.hpp"
#include "win32/path.hpp"

io::Result<std::wstring> to_widestring(const std::string &s)
{
    int required = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (required == 0)
    {
        return io::Result<std::wstring>::err(io::Error::last_os_error());
    }

    std::wstring result(required - 1, L'\0'); // exclude null terminator
    if (MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, result.data(), required) == 0)
    {
        return io::Result<std::wstring>::err(io::Error::last_os_error());
    }

    return io::Result<std::wstring>::ok(std::move(result));
}

io::Result<path::PathBuf> get_long_path(path::PathBuf &&path, bool prefer_verbatim)
{
    // Normally the MAX_PATH is 260 UTF-16 code units (including the NULL).
    // However, for APIs such as CreateDirectory[1], the limit is 248.
    //
    // [1]: https://docs.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createdirectorya#parameters
    constexpr size_t LEGACY_MAX_PATH = 248;

    // UTF-16 encoded code points, used in parsing and building UTF-16 paths.
    // All of these are in the ASCII range so they can be cast directly to `u16`.
    constexpr wchar_t SEP = L'\\';
    constexpr wchar_t ALT_SEP = L'/';
    constexpr wchar_t QUERY = L'?';
    constexpr wchar_t COLON = L':';
    constexpr wchar_t DOT = L'.';
    constexpr wchar_t U = L'U';
    constexpr wchar_t N = L'N';
    constexpr wchar_t C = L'C';

    const std::wstring VERBATIM_PREFIX = L"\\\\?\\";
    const std::wstring NT_PREFIX = L"\\??\\";
    const std::wstring UNC_PREFIX = L"\\\\?\\UNC\\";

    std::wstring path_str = path.wstring();

    // Early return for paths that are already verbatim or empty.
    if (path_str.starts_with(VERBATIM_PREFIX) ||
        path_str.starts_with(NT_PREFIX) ||
        path_str == L"\0")
    {
        return io::Result<path::PathBuf>::ok(std::move(path));
    }
    else if (path_str.length() < LEGACY_MAX_PATH)
    {
        // Early return if an absolute path is < 260 UTF-16 code units.
        // This is an optimization to avoid calling `GetFullPathNameW` unnecessarily.
        if (path_str.length() >= 2)
        {
            wchar_t first = path_str[0];
            wchar_t second = path_str[1];

            // Starts with `D:`, `D:\`, `D:/`, etc.
            // Does not match if the path starts with a `\` or `/`.
            if (second == COLON && first != SEP && first != ALT_SEP)
            {
                if (path_str.length() == 2 ||
                    path_str[2] == SEP ||
                    path_str[2] == ALT_SEP)
                {
                    return io::Result<path::PathBuf>::ok(std::move(path));
                }
            }

            // Starts with `\\`, `//`, etc
            if ((first == SEP || first == ALT_SEP) &&
                (second == SEP || second == ALT_SEP))
            {
                return io::Result<path::PathBuf>::ok(std::move(path));
            }
        }
    }

    // Firstly, get the absolute path using `GetFullPathNameW`.
    // https://docs.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getfullpathnamew
    DWORD buffer_size = GetFullPathNameW(path_str.c_str(), 0, nullptr, nullptr);
    if (buffer_size == 0)
    {
        return io::Result<path::PathBuf>::err(io::Error::last_os_error());
    }

    std::vector<wchar_t> buffer(buffer_size);
    DWORD result = GetFullPathNameW(path_str.c_str(), buffer_size, buffer.data(), nullptr);
    if (result == 0 || result >= buffer_size)
    {
        return io::Result<path::PathBuf>::err(io::Error::last_os_error());
    }

    // Convert buffer to wstring (excluding null terminator)
    std::wstring absolute(buffer.data(), result);
    std::wstring final_path;

    // Only prepend the prefix if needed.
    if (prefer_verbatim || absolute.length() + 1 >= LEGACY_MAX_PATH)
    {
        // Secondly, add the verbatim prefix. This is easier here because we know the
        // path is now absolute and fully normalized (e.g. `/` has been changed to `\`).

        std::wstring_view absolute_view = absolute;
        std::wstring_view prefix;

        if (absolute.length() >= 3 && absolute[1] == COLON && absolute[2] == SEP)
        {
            prefix = VERBATIM_PREFIX;
        }
        else if (absolute.length() >= 4 &&
                 absolute[0] == SEP && absolute[1] == SEP &&
                 absolute[2] == DOT && absolute[3] == SEP)
        {
            absolute_view = absolute_view.substr(4);
            prefix = VERBATIM_PREFIX;
        }
        else if (absolute.length() >= 4 &&
                 ((absolute[0] == SEP && absolute[1] == SEP &&
                   absolute[2] == QUERY && absolute[3] == SEP) ||
                  (absolute[0] == SEP && absolute[1] == QUERY &&
                   absolute[2] == QUERY && absolute[3] == SEP)))
        {
            // Leave \\?\ and \??\ as-is.
            prefix = L"";
        }
        else if (absolute.length() >= 2 &&
                 absolute[0] == SEP && absolute[1] == SEP)
        {
            absolute_view = absolute_view.substr(2);
            prefix = UNC_PREFIX;
        }
        else
        {
            // Anything else we leave alone.
            prefix = L"";
        }

        final_path.reserve(prefix.length() + absolute_view.length() + 1);
        final_path.append(prefix);
        final_path.append(absolute_view);
    }
    else
    {
        final_path.reserve(absolute.length() + 1);
        final_path = absolute;
    }

    return io::Result<path::PathBuf>::ok(final_path);
}
