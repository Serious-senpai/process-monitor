#pragma once

#include "pch.hpp"

/**
 * @brief Abstract base class for custom errors.
 *
 * Typically, errors appear as the second template parameter to @ref Result<T, E>.
 *
 * @see https://doc.rust-lang.org/std/error/trait.Error.html
 */
class Error : public std::exception
{
public:
    /**
     * @brief Default destructor for error struct.
     */
    virtual ~Error() = default;

    /**
     * @brief Returns a human-readable description of the error.
     *
     * @return A C-style string describing the error.
     */
    virtual const char *message() const noexcept = 0;

    /**
     * @brief Returns the underlying cause of this error, if any.
     *
     * @return A pointer to the source error, or `nullptr` if there is no underlying cause.
     */
    virtual const Error *source() const noexcept;

    /**
     * @brief Format the error to output streams.
     */
    friend std::ostream &operator<<(std::ostream &os, const Error &err);

    virtual const char *what() const noexcept override;
};
