#pragma once

#include "pch.hpp"

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

    explicit Result(T &&value) : _data(std::move(value)), NonConstructible(NonConstructibleTag::TAG) {}
    explicit Result(E &&error) : _data(std::move(error)), NonConstructible(NonConstructibleTag::TAG) {}

public:
    using Value = T;
    using Error = E;

    static Result ok(T &&value) { return Result(std::move(value)); }
    static Result err(E &&error) { return Result(std::move(error)); }

    bool is_ok() const noexcept { return std::holds_alternative<T>(_data); }
    bool is_err() const noexcept { return std::holds_alternative<E>(_data); }

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

    template <typename FT, typename FE>
    std::variant<std::invoke_result_t<FT, T &>, std::invoke_result_t<FE, E &>> match(FT &&on_ok, FE &&on_err)
    {
        if (is_ok())
        {
            return std::invoke(std::forward<FT>(on_ok), std::get<T>(_data));
        }
        else
        {
            return std::invoke(std::forward<FE>(on_err), std::get<E>(_data));
        }
    }
};
