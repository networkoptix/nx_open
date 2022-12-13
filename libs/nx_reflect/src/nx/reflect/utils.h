// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

namespace nx::reflect::detail {

/**
 * Types in this namespace are introduced to get rid of the <type_traits> and <utility> includes.
 */

template<bool B, typename T = void>
struct enable_if {};

template<typename T>
struct enable_if<true, T> { using type = T; };

template<bool B, typename T = void>
using enable_if_t = typename enable_if<B, T>::type;

//-------------------------------------------------------------------------------------------------

template<typename T> struct remove_reference { using type = T; };
template<typename T> struct remove_reference<T&> { using type = T; };
template<typename T> struct remove_reference<T&&> { using type = T; };

template<typename T>
using remove_reference_t = typename remove_reference<T>::type;

template<typename T>
constexpr T&& forward(remove_reference_t<T>& t) noexcept
{
    return static_cast<T&&>(t);
}

template<typename T>
constexpr T&& forward(remove_reference_t<T>&& t) noexcept
{
    return static_cast<T&&>(t);
}

//-------------------------------------------------------------------------------------------------

template<typename...> struct make_void { typedef void type; };
template<typename... Args> using void_t = typename make_void<Args...>::type;

template <typename, typename>
constexpr bool is_same_v = false;

template <typename T>
constexpr bool is_same_v<T, T> = true;

//-------------------------------------------------------------------------------------------------

class string_view
{
private:
    const char* m_data = nullptr;
    int m_length = 0;

    static constexpr int strlen(const char* str)
    {
        int len = 0;
        while (str[len])
            ++len;
        return len;
    }

public:
    constexpr string_view() = default;
    constexpr string_view(const char* data, int length): m_data(data), m_length(length) {}
    constexpr string_view(const char* data): string_view(data, strlen(data)) {}

    constexpr const char* data() const { return m_data; }
    constexpr int length() const { return m_length; }

    constexpr bool empty() const { return m_length == 0; }

    constexpr string_view substr(int pos, int length) const
    {
        return string_view(m_data + pos, length);
    }

    constexpr char operator[](int index) const { return m_data[index]; }

    constexpr bool operator<(const string_view& other) const
    {
        for (int i = 0; i < m_length && i < other.m_length; ++i)
        {
            if (m_data[i] != other.m_data[i])
                return m_data[i] < other.m_data[i];
        }
        return m_length < other.m_length;
    }

    constexpr bool operator==(const string_view& other) const
    {
        for (int i = 0; i < m_length && i < other.m_length; ++i)
        {
            if (m_data[i] != other.m_data[i])
                return false;
        }
        return m_length == other.m_length;
    }

    constexpr bool operator!=(const string_view& other) const { return !(*this == other); }
};

//-------------------------------------------------------------------------------------------------

template<typename T> struct Decay { using Type = T; };
template<typename T> struct Decay<const T&> { using Type = T; };
template<typename T> struct Decay<T&> { using Type = T; };
template<typename T> struct Decay<const T> { using Type = T; };

// Getter is a `T(Class::*) const`.
template<typename Getter>
struct GetterReturnType {};

template<typename C, typename T>
struct GetterReturnType<T (C::*)() const> { using Type = typename Decay<T>::Type; };

template<typename C, typename T>
struct GetterReturnType<T (C::*)() const noexcept> { using Type = typename Decay<T>::Type; };

// Setter is a `void(Class::*)(T)`.
template<typename Setter>
struct SetterReturnType {};

template<typename C, typename T>
struct SetterReturnType<void(C::*)(T)> { using Type = typename Decay<T>::Type; };

template<typename C, typename T>
struct SetterReturnType<void(C::*)(T) noexcept> { using Type = typename Decay<T>::Type; };

} // namespace nx::reflect::detail
