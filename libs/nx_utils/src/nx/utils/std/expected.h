// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <utility>

namespace nx::utils {

/**
 * Constructs an error object to be used with `expected`.
 */
template<typename E>
class unexpected
{
public:
    unexpected(E e): m_e(std::move(e)) {}

    constexpr E& error()& { return m_e; }
    constexpr const E& error() const& { return m_e; }
    constexpr E&& error()&& { return std::move(m_e); }
    constexpr const E&& error() const&& { return std::move(m_e); }

private:
    E m_e;
};

/**
 * Partial implementation of "expected" c++23 proposal.
 * See http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2021/p0323r11.html
 * To be used as a return type of a function that wants to report either value or
 * an error.
 * So, here T is a return value type, E is an error type.
 * The usage is very similar to `std::optional<T>`, but in case of `!expected` being `false`,
 * `expected::error()` method can be used to get the error.
 * Note: T and E can have the same type since `unexpected<E>` is always used to construct an error.
 */
template<typename T, typename E>
class expected
{
public:
    expected(T t): m_t(std::move(t)) {}
    expected(unexpected<E> e): m_e(std::move(e)) {}

    expected(const expected&) = default;
    expected(expected&&) = default;

    expected& operator=(const expected&) = default;
    expected& operator=(expected&&) = default;

    expected& operator=(const T& t)
    {
        *this = expected<T, E>(t);
        return *this;
    }

    expected& operator=(T&& t)
    {
        *this = expected<T, E>(std::move(t));
        return *this;
    }

    expected& operator=(const unexpected<E>& e)
    {
        *this = expected<T, E>(e);
        return *this;
    }

    expected& operator=(unexpected<E>&& e)
    {
        *this = expected<T, E>(std::move(e));
        return *this;
    }

    constexpr const T* operator->() const noexcept { return &*m_t; }
    constexpr T* operator->() noexcept { return &*m_t; }

    constexpr const T& operator*() const& noexcept { return *m_t; }
    constexpr T& operator*() & noexcept { return *m_t; }
    constexpr const T&& operator*() const&& noexcept { return std::move(*m_t); }
    constexpr T&& operator*() && noexcept { return std::move(*m_t); }

    constexpr explicit operator bool() const noexcept { return static_cast<bool>(m_t); }
    constexpr bool has_value() const noexcept { return static_cast<bool>(m_t); }

    constexpr T& value()& { return m_t.value(); }
    constexpr const T& value() const& { return m_t.value(); }
    constexpr T&& value()&& { return std::move(m_t.value()); }
    constexpr const T&& value() const&& { return std::move(m_t.value()); }

    constexpr E& error()& { return m_e->error(); }
    constexpr const E& error() const& { return m_e->error(); }
    constexpr E&& error()&& { return std::move(m_e->error()); }
    constexpr const E&& error() const&& { return std::move(m_e->error()); }

private:
    std::optional<T> m_t;
    std::optional<unexpected<E>> m_e;
};

} // namespace nx::utils
