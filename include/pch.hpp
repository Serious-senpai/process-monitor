#pragma once

#include <cstdint>
#include <exception>
#include <filesystem>
#include <format>
#include <functional>
#include <iostream>
#include <span>
#include <string>
#include <variant>

#ifdef _WIN32

#include <windows.h>

#elif defined(__linux__)

#define _FILE_OFFSET_BITS 64

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>

#endif

/**
 * @brief A tag type used to control explicit construction of @ref NonConstructible.
 */
struct NonConstructibleTag final
{
private:
    explicit constexpr NonConstructibleTag() noexcept = default;

public:
    /**
     * @brief A singleton tag instance used to construct @ref NonConstructible types.
     */
    static const NonConstructibleTag TAG;
};

/**
 * @brief A base class that disables C++'s annoying implicit constructors and copy semantics.
 *
 * In Rust, every struct behaves *explicitly* - there are no implicit default constructors,
 * no implicit copies, and you always decide when to clone or move.
 *
 * C++, on the other hand, loves generating things for you that you never asked for:
 * default constructors, copy constructors, assignment operators,... even when they make no sense.
 *
 * This @ref NonConstructible base class disables:
 * - Implicit default construction
 * - Copy construction and copy assignment
 *
 * But allows:
 * - Explicit construction by derived types
 * - Move semantics (move construction and move assignment)
 *
 * Use it as a base class for types that should behave more like Rust structs - that is,
 * types that cannot be *accidentally* copied or default-constructed.
 *
 * Example:
 * @code
 * struct MyData : NonConstructible {
 *     int x;
 *     explicit MyData(int value) : x(value) {}
 * };
 *
 * int main() {
 *     MyData a{42};
 *     // MyData b;             // error: deleted default constructor
 *     // MyData c = a;         // error: copy constructor deleted
 *     MyData d = std::move(a); // move is allowed
 * }
 * @endcode
 *
 * @note
 * Rust forces you to be explicit - and that's *good*.
 * In C++, you have to *fight the compiler* to achieve the same sanity.
 */
class NonConstructible
{
public:
    /** @brief Disable implicit default construction */
    NonConstructible() = delete;
    /** @brief Disable copying */
    NonConstructible(const NonConstructible &) = delete;
    /** @brief Disable copy assignment */
    NonConstructible &operator=(const NonConstructible &) = delete;
    /** @brief Allow moving */
    NonConstructible(NonConstructible &&) noexcept = default;
    /** @brief Allow move assignment */
    NonConstructible &operator=(NonConstructible &&) noexcept = default;

protected:
    /**
     * @brief Protected constructor - derived types can explicitly construct.
     *
     * Pass @ref NonConstructibleTag::TAG as the only parameter.
     */
    explicit NonConstructible(NonConstructibleTag) noexcept {}
};
