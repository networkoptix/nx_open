// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <string>
#include <optional>

#pragma once

namespace nx::reflect {

struct NX_REFLECT_API DeserializationResult
{
    bool success = true;
    std::string errorDescription;

    /**
     * A fragment of an input string which wasn't parsed
     */
    std::string firstBadFragment;

    /**
     * A name of the least common ancestor of object type of the field which wasn't deserialized,
     * if exists.
     */
    std::optional<std::string> firstNonDeserializedField;

    DeserializationResult(bool result);
    DeserializationResult() = default;
    DeserializationResult(
        bool result,
        std::string errorDescription,
        std::string badFragment,
        std::optional<std::string> notDeserializedField = std::nullopt);

    operator bool() const noexcept;
};

namespace detail {

/**
 * Types in this namespace are introduced to get rid of "#include <type_traits>"
 * and "#include <utility>".
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

} // namespace detail

} // namespace nx::reflect
