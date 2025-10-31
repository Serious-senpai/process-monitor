#pragma once

#include "pch.hpp"

template <typename T, typename E>
class Result
{
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

private:
    std::variant<T, E> _data;

    explicit Result(T &&value) : _data(std::move(value)) {}
    explicit Result(E &&error) : _data(std::move(error)) {}
};
