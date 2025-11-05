#pragma once

#include "io.hpp"
#include "path.hpp"

/**
 * @brief Convert a string to UTF-16 via
 * [`MultiByteToWideChar`](https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-multibytetowidechar).
 */
io::Result<std::wstring> to_widestring(const std::string &s);

/**
 * @brief Gets a normalized absolute path that can bypass path length limits.
 *
 * @see https://github.com/rust-lang/rust/blob/8182085617878610473f0b88f07fc9803f4b4960/library/std/src/sys/path/windows.rs#L90-L186
 */
io::Result<path::PathBuf> get_long_path(path::PathBuf &&path, bool prefer_verbatim);
