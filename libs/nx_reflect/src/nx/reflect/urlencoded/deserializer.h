// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <unordered_map>
#include <optional>

#include <nx/reflect/generic_visitor.h>
#include <nx/reflect/instrument.h>
#include <boost/algorithm/string.hpp>

#include "../from_string.h"
#include "../type_utils.h"

namespace nx::reflect::urlencoded {

template<typename T>
struct IsOptional: std::false_type
{
};

template<typename T>
struct IsOptional<std::optional<T>>: std::true_type
{
};

template<typename T>
inline constexpr bool IsOptionalV = IsOptional<T>::value;

//-------------------------------------------------------------------------------------------------

template<typename T, typename = std::void_t<>>
struct hasFromString: std::false_type
{
};

template<typename T>
struct hasFromString<
    T,
    std::void_t<decltype(nx::reflect::detail::fromString(
        std::declval<std::string_view>(), std::declval<T>()))>>: std::true_type
{
};

template<typename T>
inline constexpr bool hasFromStringV = hasFromString<T>::value;

//-------------------------------------------------------------------------------------------------

template<typename T>
inline constexpr bool IsStringAlikeV =
    std::is_convertible_v<std::string, T> ||
    std::is_convertible_v<std::string_view, T>
    || reflect::detail::IsQByteArrayAlikeV<T> ||
    reflect::detail::HasFromBase64V<T> ||
    reflect::detail::HasFromStdStringV<T> ||
    reflect::detail::HasFromStringV<T>;

namespace detail {

NX_REFLECT_API std::tuple<std::string, bool> decode(const std::string_view& str);

NX_REFLECT_API std::tuple<std::string_view, bool> trancateCurlBraces(const std::string_view& str);

NX_REFLECT_API std::tuple<std::vector<std::string_view>, bool> tokenizeRequest(
    const std::string_view& request, char delimiter);

template<typename Data>
class UrlencodedDeserializer;

template<typename T>
std::tuple<T, bool> tryDeserialize(const std::string_view& str);

// this function allow to avoid compilation errors for not supported types
template<typename T>
std::tuple<T, bool> defaultDeserialize(const std::string_view& str)
{
    if constexpr (IsStdChronoDurationV<T>)
    {
        T result;
        bool parsedSuccess = nx::reflect::detail::fromString(str, &result);
        return {result, parsedSuccess};
    }
    return {T(), false};
}

template<typename T>
constexpr bool isInternalDeserializable()
{
    constexpr bool isContainer =
        (IsSequenceContainerV<T> ||
        IsSetContainerV<T> ||
        IsUnorderedSetContainerV<T>) 
        &&!IsStringAlikeV<T>;
    constexpr bool IsAssociativeContainer =
        (IsAssociativeContainerV<T> && !IsSetContainerV<T>)
            || (IsUnorderedAssociativeContainerV<T> && !IsUnorderedSetContainerV<T>);
    return std::is_same_v<T,std::string> ||
        IsOptionalV<T> ||
        IsInstrumentedV<T> ||
        std::is_arithmetic_v<T> ||
        isContainer ||
        IsAssociativeContainer ||
        std::is_enum_v<T>;
}

template<typename T>
std::tuple<T, bool> deserialize(
    const std::string_view& str, std::enable_if_t<std::is_same_v<T, std::string>>* = nullptr)
{
    return decode(str);
}

template<typename T>
std::tuple<T, bool> deserialize(
    const std::string_view& str, std::enable_if_t<IsOptionalV<T>>* = nullptr)
{
    return tryDeserialize<typename T::value_type>(str);
}

template<typename T>
std::tuple<T, bool> deserialize(
    const std::string_view& str, std::enable_if_t<IsInstrumentedV<T>>* = nullptr)
{
    T data;
    UrlencodedDeserializer<T> visitor(str, data);
    nx::reflect::visitAllFields<T>(visitor);
    return {data, !visitor.deserializationFailed()};
}

template<typename T>
std::tuple<T, bool> deserialize(
    const std::string_view& str, std::enable_if_t<std::is_same_v<T, bool>>* = nullptr)
{
    std::string strCopy(str);
    boost::algorithm::to_lower(strCopy);
    if (strCopy == "true")
        return {true, true};
    else if (strCopy == "false")
        return {false, true};
    return {false, false};
}

template<typename T>
std::tuple<T, bool> deserialize(
    const std::string_view& str, std::enable_if_t<std::is_enum_v<T>>* = nullptr)
{
    T result;
    bool parsedSuccess = nx::reflect::fromString(str, &result);
    return {result, parsedSuccess};
}

template<typename T>
std::tuple<T, bool> deserialize(
    const std::string_view& str,
    std::enable_if_t<std::is_arithmetic_v<T> && !std::is_same_v<T, bool>>* = nullptr)
{
    // TO DO: this may be done better
    return {atof(std::string(str).c_str()), true};
}

template<typename T>
std::tuple<T, bool> deserialize(
    const std::string_view& str,
    std::enable_if_t<
        (IsSequenceContainerV<T> ||
        IsSetContainerV<T> ||
        IsUnorderedSetContainerV<T>) 
        &&!IsStringAlikeV<T>
    >* = nullptr)
{
    const auto& [fieldStr, trancatedSuccess] = trancateCurlBraces(str);
    if (!trancatedSuccess)
        return {T(), false};
    const auto& [strValues, tokenizedSuccess] = tokenizeRequest(fieldStr, ',');
    if (!tokenizedSuccess)
        return {T(), false};
    T data;
    for (const auto& str: strValues)
    {
        const auto& [element, success] = tryDeserialize<typename T::value_type>(str);
        if (!success)
            return {T(), false};
        std::inserter(data, data.end()) = std::move(element);
    }
    return {data, true};
}

template<typename T>
std::tuple<T, bool> deserialize(
    const std::string_view& str,
    std::enable_if_t<
        IsAssociativeContainerV<T> &&
        !IsSetContainerV<T> &&
        !IsUnorderedSetContainerV<T>
    >* = nullptr)
{
    const auto& [fieldStr, trancatedSuccess] = trancateCurlBraces(str);
    if (!trancatedSuccess)
        return {T(), false};
    const auto& [strValues, tokenizedSuccess] = tokenizeRequest(fieldStr, '&');
    if (!tokenizedSuccess)
        return {T(), false};
    T data;
    for (const auto& token: strValues)
    {
        auto pos = token.find('=');
        if (pos == std::string::npos)
            return {data, false};

        const auto& [key, keySuccess] = tryDeserialize<typename T::key_type>(token.substr(0, pos));
        const auto& [value, valueSuccess] = tryDeserialize<typename T::value_type::second_type>(
            token.substr(pos + 1, token.length() - pos - 1));
        if (!keySuccess || !valueSuccess)
            return {T(), false};

        data.emplace(key, value);
    }
    return {data, true};
}

template<typename Data>
class UrlencodedDeserializer: public nx::reflect::GenericVisitor<UrlencodedDeserializer<Data>>
{
public:
    UrlencodedDeserializer(const std::string_view& request, Data& data);

    template<typename WrappedField>
    void visitField(const WrappedField& field)
    {
        if (m_deserializationFailed)
            return;
        if (!m_request.count(field.name()))
            return;
        auto& fieldStr = m_request[field.name()];
        if constexpr (IsInstrumentedV<typename WrappedField::Type>)
        {
            bool trancatedSuccess;
            std::tie(fieldStr, trancatedSuccess) = trancateCurlBraces(m_request[field.name()]);
            if (!trancatedSuccess)
            {
                m_deserializationFailed = true;
                return;
            }
        }

        const auto& [data, result] = tryDeserialize<typename WrappedField::Type>(fieldStr);
        if (!result)
        {
            m_deserializationFailed = true;
            return;
        }
        field.set(&m_data, std::move(data));
    }

    bool deserializationFailed() { return m_deserializationFailed; };

private:
    std::unordered_map<std::string, std::string> m_request;
    bool m_deserializationFailed = false;
    Data& m_data;
};

template<typename Data>
inline UrlencodedDeserializer<Data>::UrlencodedDeserializer(
    const std::string_view& request, Data& data):
    m_data(data)
{
    const auto& [requestTokenized, success] = tokenizeRequest(request, '&');
    if (!success)
    {
        m_deserializationFailed = true;
        return;
    }

    for (const auto& token: requestTokenized)
    {
        auto pos = token.find('=');
        if (pos == std::string::npos)
        {
            m_deserializationFailed = true;
            return;
        }
        const auto& [fieldName, success] = decode(token.substr(0, pos));
        if (!success)
        {
            m_deserializationFailed = true;
            return;
        }
        m_request[fieldName] = token.substr(pos + 1, token.length() - pos - 1);
    }
}

template<typename T>
std::tuple<T, bool> tryDeserialize(const std::string_view& str)
{
    if constexpr (isInternalDeserializable<T>())
        return deserialize<T>(str);
    else
        return defaultDeserialize<T>(str);
}

} // namespace detail

/**
 * Deserializes urlencoded text into an object of supported type Data.
 * This is a convenience overload. See the next deserialize for details.
 * @return std::tuple<deserialized value, result>
 */
template<typename Data>
std::tuple<Data, bool /*result*/> deserialize(const std::string_view& request)
{
    return detail::tryDeserialize<Data>(request);
}

/**
 * Deserializes urlencoded data into an object of supported type Data.
 * @param data should be either instrumented type or build-in type(see isInternalDeserializable for
 * details)
 * @return true if deserialized successfully.
 * NOTE: All fields are considered optional. So, missing field is not an error.
 */
template<typename Data>
bool deserialize(const std::string_view& request, Data* data)
{
    bool success;
    std::tie(*data, success) = deserialize<Data>(request);
    return success;
}

} // namespace nx::reflect::urlencoded
