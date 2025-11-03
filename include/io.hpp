#pragma once

#include "error.hpp"
#include "pch.hpp"
#include "result.hpp"

namespace io
{
    /**
     * @brief A list specifying general categories of I/O error.
     *
     * @see https://doc.rust-lang.org/std/io/enum.ErrorKind.html
     */
    enum IoErrorKind
    {
        /** @brief A parameter was incorrect. */
        InvalidInput,

        /**
         * @brief Error returned by the OS.
         *
         * @todo Copy Rust internal implementation?
         */
        Os,

        /** @brief A custom error that does not fall under any other I/O error kind. */
        Other,
    };

    /**
     * @brief Enumeration of possible methods to seek within an I/O object.
     *
     * @see https://doc.rust-lang.org/std/io/enum.SeekFrom.html
     */
    struct SeekFrom
    {
        enum Type
        {
            Start,
            End,
            Current
        } type;
        int64_t offset;
    };

    /**
     * @brief The error type for I/O operations of the @ref Read, @ref Write, @ref Seek, and associated interfaces.
     *
     * @see https://doc.rust-lang.org/std/io/struct.Error.html
     */
    class IoError : public NonConstructible,
                    public Error
    {
    private:
        IoErrorKind _kind;
        std::string _message;

    public:
        /**
         * @brief Creates a new I/O error from a known kind of error as well as an arbitrary error payload.
         */
        explicit IoError(IoErrorKind kind, std::string &&message);

        /**
         * @brief Creates a new I/O error from an arbitrary error payload.
         */
        static IoError other(std::string &&message);

        const char *message() const noexcept override;
    };

    template <typename T>
    using Result = result::Result<T, IoError>;

    class Read
    {
    public:
        virtual Result<size_t> read(std::span<char> buffer) = 0;
    };

    class Write
    {
    public:
        virtual Result<size_t> write(std::span<const char> buffer) = 0;
        virtual Result<std::monostate> flush() = 0;
    };

    class Seek
    {
    public:
        virtual Result<uint64_t> seek(SeekFrom position) = 0;
    };
}
