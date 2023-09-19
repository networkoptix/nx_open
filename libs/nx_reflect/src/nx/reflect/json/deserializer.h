// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <optional>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <variant>

#include <rapidjson/document.h>

#include <nx/reflect/enum_string_conversion.h>
#include <nx/reflect/from_string.h>
#include <nx/reflect/generic_visitor.h>
#include <nx/reflect/instrument.h>
#include <nx/reflect/type_utils.h>

#include "utils.h"
#include "../tags.h"

namespace nx::reflect {

namespace json {

enum class DeserializationFlag
{
    none = 0,
    ignoreFieldTypeMismatch = 1, // All errors from custom deserializers will be skipped too
};

} // namespace json

/**
 * Not using json::detail so that ADL does not see nx::reflect::json::deserialize call.
 */
namespace json_detail {

// NOTE: This class is introduced to simplify forward-declaring custom JSON deserialization functions.
// Subject to change since it is not a real deserialization context currently.
struct DeserializationContext
{
    const rapidjson::Value& value;
    int flags = 0;
};

template<typename T>
struct IsBuiltInJsonValueType
{
    static constexpr bool value =
        std::is_integral_v<T> |
        std::is_floating_point_v<T> |
        std::is_same_v<T, std::string>|
        std::is_same_v<T, std::nullptr_t>;
};

template<typename... U> inline constexpr bool IsBuiltInJsonTypeV =
    IsBuiltInJsonValueType<U...>::value;

template<typename T>
inline constexpr bool IsStringAlikeV =
    std::is_convertible_v<std::string, T> ||
    std::is_convertible_v<std::string_view, T> ||
    reflect::detail::IsQByteArrayAlikeV<T> ||
    reflect::detail::HasFromBase64V<T> ||
    reflect::detail::HasFromStdStringV<T> ||
    reflect::detail::HasFromStringV<T>;

//-------------------------------------------------------------------------------------------------

template<typename Data> class Deserializer;

template<typename T> DeserializationResult deserializeValue(
    const DeserializationContext& ctx, T* data);

template<
    typename Data,
    typename = std::enable_if_t<IsInstrumentedV<Data>>
>
DeserializationResult deserialize(const DeserializationContext& ctx, Data* data)
{
    Deserializer deserializer(ctx, data);
    nx::reflect::visitAllFields<Data>(deserializer);
    return deserializer.ok();
}

template<typename T>
DeserializationResult deserialize(
    const DeserializationContext& ctx,
    T* data,
    std::enable_if_t<std::is_floating_point_v<T>>* = nullptr)
{
    if (ctx.value.IsDouble())
        *data = (T) ctx.value.GetDouble();
    else if (ctx.value.IsInt64())
        *data = (T) ctx.value.GetInt64();
    else if (ctx.value.IsString())
        *data = (T) std::stod(std::string(ctx.value.GetString(), ctx.value.GetStringLength()));

    return DeserializationResult(true);
}

template<typename T, typename... Args>
DeserializationResult deserializeVariantType(
    const DeserializationContext& ctx, std::variant<Args...>* data)
{
    T typedData;
    auto r = deserializeValue(ctx, &typedData);
    if (r)
        *data = typedData;

    return r;
}

template<typename... Args>
DeserializationResult deserialize(const DeserializationContext& ctx, std::variant<Args...>* data)
{
    return (... || deserializeVariantType<Args>(ctx, data));
}

template<typename... Args>
DeserializationResult deserialize(
    const DeserializationContext& ctx,
    std::chrono::duration<Args...>* data)
{
    if (ctx.value.IsNumber())
    {
        *data = std::chrono::duration<Args...>(ctx.value.GetInt64());
    }
    else if (ctx.value.IsString())
    {
        *data = std::chrono::duration<Args...>(std::stoll(
            std::string(ctx.value.GetString(), ctx.value.GetStringLength())));
    }
    else
    {
        *data = std::chrono::duration<Args...>();
        return {
            false,
            "Either a number or a string is expected for an std::chrono::duration value",
            getStringRepresentation(ctx.value)};
    }

    return DeserializationResult(true);
}

template<typename... Args>
DeserializationResult deserialize(
    const DeserializationContext& ctx,
    std::chrono::time_point<Args...>* data)
{
    std::chrono::milliseconds ms;
    *data = std::chrono::time_point<Args...>();
    auto deserializationResult = deserialize(ctx, &ms);
    if (!deserializationResult.success)
        return deserializationResult;

    *data = std::chrono::time_point<Args...>(ms);
    return DeserializationResult(true);
}

template<typename C>
DeserializationResult deserialize(
    const DeserializationContext& ctx,
    C* data,
    std::enable_if_t<
        (IsSequenceContainerV<C> ||
        IsSetContainerV<C> ||
        IsUnorderedSetContainerV<C>) &&
        !IsStringAlikeV<C>
    >* = nullptr)
{
    *data = C();
    if (!ctx.value.IsArray())
        return {false, "Array is expected", getStringRepresentation(ctx.value)};

    for (rapidjson::SizeType i = 0; i < ctx.value.Size(); ++i)
    {
        typename C::value_type element;
        const auto deserializationResult =
            deserializeValue(DeserializationContext{ctx.value[i], ctx.flags}, &element);
        if (!deserializationResult.success)
        {
            if (!(ctx.flags & (int) json::DeserializationFlag::ignoreFieldTypeMismatch))
                return deserializationResult;
            continue;
        }
        std::inserter(*data, data->end()) = std::move(element);
    }
    return DeserializationResult(true);
}

template<typename C, typename Key, typename = std::void_t<>>
struct HasSquareBracketOperator: std::false_type {};

template<typename C, typename Key>
struct HasSquareBracketOperator<
    C,
    Key,
    std::void_t<decltype(std::declval<C>()[std::declval<Key>()])>
>: std::true_type
{
};

template<typename... Args>
inline constexpr bool HasSquareBracketOperatorV = HasSquareBracketOperator<Args...>::value;

template<typename C>
DeserializationResult deserialize(
    const DeserializationContext& ctx,
    C* data,
    std::enable_if_t<
        (IsAssociativeContainerV<C> && !IsSetContainerV<C>)
            || (IsUnorderedAssociativeContainerV<C> && !IsUnorderedSetContainerV<C>)
    >* = nullptr)
{
    if (!ctx.value.IsObject())
    {
        return DeserializationResult{
            false,
            "Associative container value should be an object",
            getStringRepresentation(ctx.value)};
    }

    for (auto it = ctx.value.MemberBegin(); it != ctx.value.MemberEnd(); ++it)
    {
        typename C::mapped_type element;
        const auto deserializationResult =
            deserializeValue(DeserializationContext{it->value, ctx.flags}, &element);
        if (!deserializationResult.success)
        {
            if (!(ctx.flags & (int) json::DeserializationFlag::ignoreFieldTypeMismatch))
                return deserializationResult;
            else
                continue;
        }

        typename C::key_type key;
        if (!nx::reflect::fromString(
                std::string_view(it->name.GetString(), it->name.GetStringLength()),
                &key))
        {
            if (!(ctx.flags & (int) json::DeserializationFlag::ignoreFieldTypeMismatch))
            {
                return DeserializationResult{
                    false,
                    "In a key-value container a key should be a string",
                    getStringRepresentation(ctx.value)};
            }
            continue;
        }

        if constexpr (HasSquareBracketOperatorV<C, decltype(key)>)
            (*data)[std::move(key)] = std::move(element); //< E.g., std::map
        else
            data->emplace(std::move(key), std::move(element)); //< E.g., std::multimap
    }

    return DeserializationResult(true);
}

template<typename T, typename = std::void_t<>>
struct IsDeserializable: std::false_type {};

template<typename T>
struct IsDeserializable<
    T,
    std::void_t<decltype(deserialize(std::declval<DeserializationContext>(), (T*) nullptr))>>
:
    std::true_type {};

template<typename... U> inline constexpr bool IsDeserializableV =
    IsDeserializable<U...>::value;

//-------------------------------------------------------------------------------------------------

template<typename T, bool result>
static void reportUnsupportedType()
{
    static_assert(result, "Type is not deserializable");
}

template<typename T>
DeserializationResult deserializeValue(const DeserializationContext& ctx, T* data)
{
    // Using if constexpr over SFINAE to make clear prioritization between serialization methods.
    // E.g., if type both instrumented and stringizable, then choosing instrumentation.

    if constexpr (nx::reflect::IsInstrumentedV<T>)
    {
        if (!ctx.value.IsObject())
        {
            *data = T();
            return {
                false,
                "Object value expected",
                getStringRepresentation(ctx.value)};
        }

        // ADL. Custom deserialize() overload will be invoked here if present.
        return deserialize(ctx, data);
    }
    if constexpr (std::is_same_v<T, std::string>)
    {
        *data = std::string();
        if (!ctx.value.IsString())
            return {false, "String value is expected", getStringRepresentation(ctx.value)};
        *data = std::string(ctx.value.GetString(), ctx.value.GetStringLength());
        return DeserializationResult(true);
    }
    else if constexpr (std::is_same_v<T, bool>)
    {
        *data = bool();
        if (ctx.value.IsBool())
        {
            *data = ctx.value.GetBool();
            return DeserializationResult(true);
        }
        if (ctx.value.IsString())
        {
            std::string_view str(ctx.value.GetString(), ctx.value.GetStringLength());
            if (str == "true")
            {
                *data = true;
                return DeserializationResult(true);
            }
            if (str == "false")
            {
                *data = false;
                return DeserializationResult(true);
            }
        }
        return {
            false,
            "Either a bool value or a string value \"true\" or \"false\" is expected",
            getStringRepresentation(ctx.value)};
    }
    else if constexpr (std::is_integral_v<T>)
    {
        *data = T();
        if (ctx.value.IsNumber())
        {
            *data = static_cast<T>(ctx.value.GetInt64());
            return DeserializationResult(true);
        }
        if (ctx.value.IsString())
        {
            *data = static_cast<T>(std::stoll(
                std::string(ctx.value.GetString(), ctx.value.GetStringLength())));
            return DeserializationResult(true);
        }
        return {
            false,
            "Either a number or a string is expected for an integral value",
            getStringRepresentation(ctx.value)};
    }
    else if constexpr (std::is_same_v<T, std::nullptr_t>)
    {
        *data = std::nullptr_t();
        if (ctx.value.IsNull())
            return DeserializationResult(true);
        return {
            false,
            "A null value is expected",
            getStringRepresentation(ctx.value)};
    }
    else if constexpr (IsInstrumentedEnumV<T>)
    {
        *data = T();
        if (ctx.value.IsString())
        {
            bool ok = enumeration::fromString<T>(
                std::string(ctx.value.GetString(), ctx.value.GetStringLength()), data);
            return DeserializationResult(ok);
        }
        if (ctx.value.IsNumber())
        {
            *data = static_cast<T>(ctx.value.GetInt64());
            return DeserializationResult(enumeration::isValidEnumValue<T>(*data));
        }
        return {
            false,
            "Either a number or a string is expected for an enum value",
            getStringRepresentation(ctx.value)};
    }
    // Checking if custom deserialize() overload is defined for a non-instrumented type T.
    // Invoking this custom method before fromString() since this deserialize() is more specific
    // to the format.
    else if constexpr (IsDeserializableV<T>)
    {
        // ADL call.
        return deserialize(ctx, data);
    }
    else if constexpr (
        std::is_object_v<T> &&
        !nx::reflect::IsInstrumented<T>::value &&
        useStringConversionForSerialization((const T*) nullptr))
    {
        *data = T();
        if (!ctx.value.IsString())
        {
            return {
                false,
                "String value is expected for an object",
                getStringRepresentation(ctx.value)};
        }
        if (!nx::reflect::fromString(ctx.value.GetString(), data))
        {
            *data = T();
            return {
                false,
                "Can't parse the string (custom parser failed)",
                getStringRepresentation(ctx.value)};
        }
        return DeserializationResult(true);
    }
    else
    {
        reportUnsupportedType<T, false>();
        return {false, "Unknown type", getStringRepresentation(ctx.value)};
    }
}

template<typename T>
DeserializationResult deserializeValue(const DeserializationContext& ctx, std::optional<T>* data)
{
    T val;
    auto result = deserializeValue(ctx, &val);
    if (result)
        *data = std::move(val);
    return result;
}

template<typename Data>
class Deserializer:
    public nx::reflect::GenericVisitor<Deserializer<Data>>
{
public:
    Deserializer(const DeserializationContext& ctx, Data* data):
        m_ctx(ctx),
        m_data(data)
    {
    }

    template<typename WrappedField>
    void visitField(const WrappedField& field)
    {
        if (!m_deserealizationResult.success)
            return;

        auto deserializationResult =
            deserializeField(m_ctx, field, (typename WrappedField::Type*) nullptr);
        if (!deserializationResult
            && !(m_ctx.flags & (int) json::DeserializationFlag::ignoreFieldTypeMismatch))
        {
            m_deserealizationResult = deserializationResult;
        }
    }

    DeserializationResult ok() const
    {
        return m_deserealizationResult;
    }

private:
    template<typename WrappedField, typename T>
    DeserializationResult deserializeField(
        const DeserializationContext& ctx,
        const WrappedField& field,
        std::optional<T>*)
    {
        std::optional<T> data;
        DeserializationResult deserializationResult(true);
        auto valueIter = ctx.value.FindMember(field.name());
        if (valueIter != ctx.value.MemberEnd())
        {
            data = T();
            auto& dataRef = *data;
            auto curDeserializationResult = deserializeValue(
                DeserializationContext{valueIter->value, ctx.flags}, &dataRef);
            if (!curDeserializationResult.success)
            {
                deserializationResult = curDeserializationResult;
                if (!deserializationResult.firstNonDeserializedField)
                    deserializationResult.firstNonDeserializedField = field.name();
            }
        }
        else
        {
            data = std::nullopt;
        }

        field.set(m_data, std::move(data));
        return deserializationResult;
    }

    template<typename WrappedField, typename T>
    DeserializationResult deserializeField(
        const DeserializationContext& ctx,
        const WrappedField& field,
        T*)
    {
        auto valueIter = ctx.value.FindMember(field.name());
        if (valueIter == ctx.value.MemberEnd())
            return DeserializationResult(true);

        T data;
        auto deserializationResult =
            deserializeValue(DeserializationContext{valueIter->value, ctx.flags}, &data);

        if (deserializationResult.success)
        {
            field.set(m_data, std::move(data));
        }
        else
        {
            // If null value cannot be converted to target value type, then just ignore it.
            if (valueIter->value.GetType() == rapidjson::kNullType)
                return DeserializationResult(true);

            if (!deserializationResult.firstNonDeserializedField)
                deserializationResult.firstNonDeserializedField = field.name();
        }

        return deserializationResult;
    }

private:
    const DeserializationContext& m_ctx;
    Data* m_data = nullptr;
    DeserializationResult m_deserealizationResult;
};

} // namespace json_detail

//-------------------------------------------------------------------------------------------------

namespace json {

using DeserializationContext = json_detail::DeserializationContext;

template<typename T>
DeserializationResult deserialize(const DeserializationContext& ctx, T* data)
{
    try
    {
        return json_detail::deserializeValue(ctx, data);
    }
    catch (const std::exception& e)
    {
        *data = T();
        return {
            false,
            "Exception during json deserialization: " + std::string(e.what()),
            json_detail::getStringRepresentation(ctx.value)};
    }
}

/**
 * Deserializes JSON text into an object of supported type Data.
 * @param data Can be any instrumented type or a container with instrumented types.
 * @return true if deserialized successfully.
 * NOTE: All JSON fields are considered optional. So, missing field in the JSON text is not an error.
 * Member-fields that are not of instrumented types and not of built-in (int, double, std::string) types
 * must be ConvertibleFromString for the deserialization to work.
 * ConvertibleFromString type supports at least one of the following methods:
 * - static T T::fromStdString(const std::string&)
 * - static T T::fromString(const std::string&)
 * - bool fromString(const std::string&, Data*)
 */
template<typename Data>
DeserializationResult deserialize(
    const std::string_view& json,
    Data* data,
    json::DeserializationFlag skipErrors = json::DeserializationFlag::none)
{
    using namespace rapidjson;

    Document document;
    document.Parse(json.data(), json.size());
    if (document.HasParseError())
        return DeserializationResult(false, json_detail::parseErrorToString(document), std::string(json));

    // Using fully qualified function name to disable ADL.
    return ::nx::reflect::json::deserialize(DeserializationContext{document, (int) skipErrors}, data);
}

/**
 * Deserializes JSON text into an object of supported type Data.
 * This is a convenience overload. See the previous deserialize for details.
 * @return std::tuple<deserialized value, result>
 */
template<typename Data>
std::tuple<Data, DeserializationResult> deserialize(
    const std::string_view& json,
    json::DeserializationFlag skipErrors = json::DeserializationFlag::none)
{
    Data data;
    auto result = deserialize<Data>(json, &data, skipErrors);
    return std::make_tuple(std::move(result.success ? data : Data()), std::move(result));
}

} // namespace json

} // namespace nx::reflect
