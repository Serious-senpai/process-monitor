#pragma once

#include "io.hpp"

/** @brief Pass a string to @ref to_widestring and then @ref get_long_path */
io::Result<std::wstring> maybe_verbatim(const char *s);

/**
 * @brief Convert a string to UTF-16 via
 * [`MultiByteToWideChar`](https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-multibytetowidechar).
 */
io::Result<std::wstring> to_widestring(const char *s);

/**
 * @brief Gets a normalized absolute path that can bypass path length limits.
 *
 * @see https://github.com/rust-lang/rust/blob/8182085617878610473f0b88f07fc9803f4b4960/library/std/src/sys/path/windows.rs#L90-L186
 */
io::Result<std::wstring> get_long_path(std::wstring &&path, bool prefer_verbatim);
