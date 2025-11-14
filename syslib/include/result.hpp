#pragma once

#include "pch.hpp"

#define SHORT_CIRCUIT(value_type, expr) ({                             \
    auto _result = (expr);                                             \
    if (_result.is_err())                                              \
    {                                                                  \
        return result::Result<                                         \
            value_type,                                                \
            std::remove_reference_t<decltype(_result.unwrap_err())>>:: \
            err(std::move(_result).into_err());                        \
    }                                                                  \
    std::move(_result).into_ok();                                      \
})

namespace result
{
    /**
     * @brief A type that represents either success or failure.
     *
     * @see https://doc.rust-lang.org/std/result/enum.Result.html
     */
    template <typename T, typename E>
    class Result : public NonConstructible
    {
    private:
        std::variant<T, E> _data;

        explicit Result(T &&value) : NonConstructible(NonConstructibleTag::TAG), _data(std::move(value)) {}
        explicit Result(E &&error) : NonConstructible(NonConstructibleTag::TAG), _data(std::move(error)) {}

    public:
        using Value = T;
        using Error = E;

        static Result ok(T &&value) { return Result(std::move(value)); }
        static Result err(E &&error) { return Result(std::move(error)); }

        bool is_ok() const noexcept { return std::holds_alternative<T>(_data); }
        bool is_err() const noexcept { return std::holds_alternative<E>(_data); }

        T into_ok() && { return std::get<T>(std::move(_data)); }
        E into_err() && { return std::get<E>(std::move(_data)); }

        T &unwrap()
        {
            if (!is_ok())
            {
                throw std::runtime_error("called unwrap() on Err");
            }
            return std::get<T>(_data);
        }

        const T &unwrap() const
        {
            if (!is_ok())
            {
                throw std::runtime_error("called unwrap() on Err");
            }
            return std::get<T>(_data);
        }

        E &unwrap_err()
        {
            if (!is_err())
            {
                throw std::runtime_error("called unwrap_err() on Ok");
            }
            return std::get<E>(_data);
        }

        const E &unwrap_err() const
        {
            if (!is_err())
            {
                throw std::runtime_error("called unwrap_err() on Ok");
            }
            return std::get<E>(_data);
        }
    };
}
