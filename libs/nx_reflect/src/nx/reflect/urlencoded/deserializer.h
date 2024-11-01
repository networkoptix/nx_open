// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <boost/algorithm/string.hpp>

#include <nx/reflect/generic_visitor.h>
#include <nx/reflect/instrument.h>
#include <nx/reflect/json/deserializer.h>

#include "../from_string.h"
#include "../serialization_utils.h"
#include "../type_utils.h"

namespace nx::reflect::urlencoded {

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

namespace detail {

NX_REFLECT_API std::tuple<std::string, DeserializationResult> decode(const std::string_view& str);

/**
 * Cuts enclosing [], {} or () if present.
 * Examples:
 * - `trimBrackets("[foo]")` => ("foo", true)
 * - `trimBrackets("{foo}")` => ("foo", true)
 * - `trimBrackets("(foo)")` => ("foo", true)
 * - `trimBrackets("foo")` => ("foo", true)
 * - `trimBrackets("f")` => "f"
 * - `trimBrackets("{foo")` => ("", false)
 * - `trimBrackets("{foo]")` => ("", false)
 */
NX_REFLECT_API std::tuple<std::string_view, DeserializationResult> trimBrackets(
    const std::string_view& str);

NX_REFLECT_API std::tuple<std::vector<std::string_view>, DeserializationResult> tokenizeRequest(
    const std::string_view& request, char delimiter);

template<typename Data>
class UrlencodedDeserializer;

template<typename T>
std::tuple<T, DeserializationResult> tryDeserialize(const std::string_view& str);

template<typename T>
std::tuple<T, DeserializationResult> defaultDeserialize(const std::string_view& str)
{
    if constexpr (std::is_same_v<std::string, T>)
        return {std::string{str}, DeserializationResult{}};

    if constexpr (reflect::detail::HasFromStdStringV<T>)
        return {T::fromStdString(str), DeserializationResult{}};

    T data;
    auto r1 = json::deserialize(str, &data, json::DeserializationFlag::fields);
    if (r1)
        return {std::move(data), std::move(r1)};

    auto r2 =
        json::deserialize('"' + std::string{str} + '"', &data, json::DeserializationFlag::fields);
    if (r2)
        return {std::move(data), std::move(r2)};

    return {T{}, std::move(r1)};
}

template<typename T>
requires IsStdChronoDurationV<T>
std::tuple<T, DeserializationResult> defaultDeserialize(const std::string_view& str)
{
    T result;
    bool parsedSuccess = nx::reflect::detail::fromString(str, &result);
    return {result, parsedSuccess};
}

// Representing timepoint in milliseconds since epoch.
template<typename T>
requires IsStdChronoTimePointV<T>
std::tuple<T, DeserializationResult> defaultDeserialize(const std::string_view& str)
{
    std::chrono::milliseconds ms;
    bool parsedSuccess = nx::reflect::detail::fromString(str, &ms);
    if (!parsedSuccess)
        return std::make_tuple(T(), false);

    return {T(std::chrono::duration_cast<typename T::duration>(ms)), true};
}

template<typename T>
constexpr bool isInternalDeserializable()
{
    constexpr bool isContainer =
        IsSequenceContainerV<T> || IsSetContainerV<T> || IsUnorderedSetContainerV<T>;

    constexpr bool isAssociativeContainer =
        (IsAssociativeContainerV<T> && !IsSetContainerV<T>) ||
        (IsUnorderedAssociativeContainerV<T> && !IsUnorderedSetContainerV<T>);

    return std::is_same_v<T,std::string> ||
        IsOptionalV<T> ||
        IsInstrumentedV<T> ||
        std::is_arithmetic_v<T> ||
        isContainer ||
        isAssociativeContainer ||
        std::is_enum_v<T>;
}

template<typename T>
std::tuple<T, DeserializationResult> deserialize(
    const std::string_view& str, std::enable_if_t<std::is_same_v<T, std::string>>* = nullptr)
{
    return decode(str);
}

template<typename T>
std::tuple<T, DeserializationResult> deserialize(
    const std::string_view& str, std::enable_if_t<IsOptionalV<T>>* = nullptr)
{
    return tryDeserialize<typename T::value_type>(str);
}

template<typename T>
std::tuple<T, DeserializationResult> deserialize(
    const std::string_view& str, std::enable_if_t<IsInstrumentedV<T>>* = nullptr)
{
    T data;
    UrlencodedDeserializer<T> visitor(str, &data);
    nx::reflect::visitAllFields<T>(visitor);
    return {data, std::move(visitor).result()};
}

template<typename T>
std::tuple<T, DeserializationResult> deserialize(
    const std::string_view& str, std::enable_if_t<std::is_same_v<T, bool>>* = nullptr)
{
    std::string strCopy(str);
    boost::algorithm::to_lower(strCopy);
    if (strCopy == "true")
        return {true, true};
    else if (strCopy == "false")
        return {false, true};
    else
        return {false, {false, "Failed to deserialze boolean", std::string{str}}};
}

template<typename T>
std::tuple<T, DeserializationResult> deserialize(
    const std::string_view& str, std::enable_if_t<std::is_enum_v<T>>* = nullptr)
{
    T result{};
    if (nx::reflect::fromString(str, &result))
        return {result, true};
    return {result, {false, "Failed to deserialize enum value", std::string{str}}};
}

template<typename T>
std::tuple<T, DeserializationResult> deserialize(
    const std::string_view& str,
    std::enable_if_t<std::is_arithmetic_v<T> && !std::is_same_v<T, bool>>* = nullptr)
{
    // TO DO: this may be done better
    return {atof(std::string(str).c_str()), true};
}

template<typename T>
std::tuple<T, DeserializationResult> deserialize(
    const std::string_view& str,
    std::enable_if_t<IsSequenceContainerV<T>
        || IsSetContainerV<T>
        || IsUnorderedSetContainerV<T>
    >* = nullptr)
{
    T data;

    auto [fieldStr, r1] = trimBrackets(str);
    if (!r1)
        return {std::move(data), std::move(r1)};

    auto [strValues, r2] = tokenizeRequest(fieldStr, ',');
    if (!r2)
        return {std::move(data), std::move(r2)};

    DeserializationResult result;
    for (const auto& value: strValues)
    {
        auto [element, r] = tryDeserialize<typename T::value_type>(value);
        if (!r)
            return {std::move(data), std::move(r)};

        std::inserter(data, data.end()) = std::move(element);
    }
    return {std::move(data), std::move(result)};
}

template<typename T>
std::tuple<T, DeserializationResult> deserialize(
    const std::string_view& str,
    std::enable_if_t<
        (IsAssociativeContainerV<T> ||
            IsUnorderedAssociativeContainerV<T>) &&
        !IsSetContainerV<T> &&
        !IsUnorderedSetContainerV<T>
    >* = nullptr)
{
    T data;

    auto [fieldStr, r1] = trimBrackets(str);
    if (!r1)
        return {std::move(data), std::move(r1)};

    auto [strValues, r2] = tokenizeRequest(fieldStr, '&');
    if (!r2)
        return {std::move(data), std::move(r2)};

    DeserializationResult result;
    for (const auto& token: strValues)
    {
        auto pos = token.find('=');
        if (pos == std::string::npos)
            return {std::move(data), {false, "Invalid key-value pair", std::string{token}}};

        auto keyStd = token.substr(0, pos);
        auto [key, keyResult] = tryDeserialize<typename T::key_type>(keyStd);
        if (!keyResult)
            return {std::move(data), std::move(keyResult)};

        auto [value, valueResult] = tryDeserialize<typename T::value_type::second_type>(
            token.substr(pos + 1, token.length() - pos - 1));
        if (!valueResult)
            return {std::move(data), std::move(valueResult)};

        data.emplace(key, value);
        result.addField(std::string{std::move(keyStd)}, std::move(valueResult.fields));
    }
    return {std::move(data), std::move(result)};
}

template<typename Data>
class UrlencodedDeserializer: public nx::reflect::GenericVisitor<UrlencodedDeserializer<Data>>
{
public:
    UrlencodedDeserializer(const std::string_view& request, Data* data);

    template<typename WrappedField>
    void visitField(const WrappedField& field)
    {
        if (!m_deserializationResult)
            return;

        using FieldType = typename WrappedField::Type;
        std::string fieldName = field.name();
        if (m_deserializationResult.hasField(fieldName))
            return;

        auto it = m_request.find(fieldName);
        if (it == m_request.end())
        {
            if constexpr (IsOptionalV<FieldType>)
                field.set(m_data, std::nullopt);
            return;
        }

        auto serializedField = it->second;
        DeserializationResult trimResult;
        if constexpr (IsOptionalV<FieldType>)
        {
            if constexpr (IsInstrumentedV<typename FieldType::value_type>)
                std::tie(serializedField, trimResult) = trimBrackets(std::move(serializedField));
        }
        else if constexpr (IsInstrumentedV<FieldType>)
        {
            std::tie(serializedField, trimResult) = trimBrackets(std::move(serializedField));
        }
        if (!trimResult)
        {
            updateFailedResult(std::move(fieldName), std::move(trimResult));
            return;
        }

        auto [data, deserializeResult] = tryDeserialize<FieldType>(serializedField);
        if (!deserializeResult)
        {
            updateFailedResult(std::move(fieldName), std::move(deserializeResult));
            return;
        }

        m_deserializationResult.addField(
            std::move(fieldName), std::move(deserializeResult.fields));
        field.set(m_data, std::move(data));
    }

    DeserializationResult result() && { return std::move(m_deserializationResult); }

private:
    void updateFailedResult(std::string fieldName, DeserializationResult result)
    {
        m_deserializationResult = std::move(result);
        if (!m_deserializationResult.firstNonDeserializedField)
            m_deserializationResult.firstNonDeserializedField = std::move(fieldName);
    }

private:
    std::unordered_map<std::string, std::string_view> m_request;
    DeserializationResult m_deserializationResult{true};
    Data* m_data;
};

template<typename Data>
inline UrlencodedDeserializer<Data>::UrlencodedDeserializer(
    const std::string_view& request, Data* data):
    m_data(data)
{
    auto [requestTokenized, tokenizeResult] = tokenizeRequest(request, '&');
    if (!tokenizeResult)
    {
        m_deserializationResult = std::move(tokenizeResult);
        return;
    }

    for (const auto& token: requestTokenized)
    {
        if (token.empty())
            continue;

        auto pos = token.find('=');
        if (pos == std::string::npos)
        {
            // Treating the token as a boolean flag.
            m_request[std::string(token)] = "true";
            continue;
        }

        auto encodedField = token.substr(0, pos);
        auto [fieldName, decodeResult] = decode(encodedField);
        if (!decodeResult)
        {
            m_deserializationResult = {false,
                "Failed to decode field name: " + std::move(decodeResult.errorDescription),
                std::move(decodeResult.firstBadFragment),
                std::string{encodedField}};
            return;
        }

        m_request[fieldName] = token.substr(pos + 1, token.length() - pos - 1);
    }
}

template<typename T>
std::tuple<T, DeserializationResult> tryDeserialize(const std::string_view& str)
{
    if constexpr (isInternalDeserializable<T>())
    {
        return deserialize<T>(str);
    }
    else if constexpr (IsStringAlikeV<T>)
    {
        bool ok = false;
        auto val = nx::reflect::fromString<T>(str, &ok);
        if (ok)
            return {std::move(val), true};
        return {std::move(val), {false, "Failed to deserialize from string", std::string{str}}};
    }
    else
    {
        return defaultDeserialize<T>(str);
    }
}

} // namespace detail

/**
 * Deserializes urlencoded text into an object of supported type Data.
 * This is a convenience overload. See the next deserialize for details.
 * @return std::tuple<deserialized value, result>
 */
template<typename Data>
std::tuple<Data, DeserializationResult> deserialize(const std::string_view& request)
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
DeserializationResult deserialize(const std::string_view& request, Data* data)
{
    DeserializationResult r;
    std::tie(*data, r) = deserialize<Data>(request);
    return r;
}

} // namespace nx::reflect::urlencoded
