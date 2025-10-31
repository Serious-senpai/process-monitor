#pragma once

#include <exception>
#include <format>
#include <functional>
#include <iostream>
#include <span>
#include <string>
#include <variant>

#ifdef _WIN32
#elif defined(__linux__)

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>

#endif

struct NonConstructibleTag final
{
    explicit constexpr NonConstructibleTag() noexcept = default;
};

inline constexpr NonConstructibleTag NON_CONSTRUCTIBLE;

/**
 * @brief A base class that disables C++'s annoying implicit constructors and copy semantics.
 *
 * In Rust, every struct behaves *explicitly* - there are no implicit default constructors,
 * no implicit copies, and you always decide when to clone or move.
 *
 * C++, on the other hand, loves generating things for you that you never asked for:
 * default constructors, copy constructors, assignment operators,... even when they make no sense.
 *
 * This `NonConstructible` base class disables:
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
 *     // MyData b;            // error: deleted default constructor
 *     // MyData c = a;        // error: copy constructor deleted
 *     MyData d = std::move(a); // move is allowed
 * }
 * @endcode
 *
 * @note
 * Rust forces you to be explicit - and that’s *good*.
 * In C++, you have to *fight the compiler* to achieve the same sanity.
 */
class NonConstructible
{
public:
    NonConstructible() = delete;                                         ///< Disable implicit default construction
    NonConstructible(const NonConstructible &) = delete;                 ///< Disable copying
    NonConstructible &operator=(const NonConstructible &) = delete;      ///< Disable copy assignment
    NonConstructible(NonConstructible &&) noexcept = default;            ///< Allow moving
    NonConstructible &operator=(NonConstructible &&) noexcept = default; ///< Allow move assignment

protected:
    /// Protected constructor - derived types can explicitly construct.
    explicit NonConstructible(NonConstructibleTag) noexcept {}
};
