// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

#include <nx/reflect/generic_visitor.h>
#include <nx/reflect/instrument.h>

#include "tags.h"
#include "to_string.h"
#include "type_utils.h"

namespace nx::reflect {

template<typename Result>
class AbstractComposer
{
public:
    virtual ~AbstractComposer() = default;

    virtual void startArray() = 0;
    virtual void endArray() = 0;

    virtual void startObject() = 0;
    virtual void endObject() = 0;

    virtual void writeBool(bool val) = 0;
    virtual void writeInt(const std::int64_t& val) = 0;
    virtual void writeFloat(const double& val) = 0;
    virtual void writeString(const std::string_view& val) = 0;
    virtual void writeRawString(const std::string_view& val) = 0;

    virtual void writeNull() = 0;

    template<typename T>
    void writeValue(T val)
    {
        if constexpr (std::is_same_v<T, bool>)
            writeBool(val);
        else if constexpr (std::is_integral_v<T>)
            writeInt(static_cast<std::int64_t>(val));
        else if constexpr (std::is_floating_point_v<T>)
            writeFloat(static_cast<double>(val));
        else if constexpr (std::is_same_v<T, std::nullptr_t>)
            writeNull();
        else if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, std::string_view>)
            writeString(val);
        else
            writeString(std::to_string(val));
    }

    template<typename... Args>
    void writeValue(
        const std::chrono::duration<Args...>& val)
    {
        if (m_serializeDurationAsNumber)
            writeValue(val.count());
        else
            writeValue(std::to_string(val.count()));
    }

    template<typename... Args>
    void writeValue(
        const std::chrono::time_point<Args...>& val)
    {
        using namespace std::chrono;
        writeValue(floor<milliseconds>(val.time_since_epoch()));
    }

    virtual void writeAttributeName(const std::string_view& name) = 0;

    virtual Result take() = 0;

    /**
     * @return The previous value.
     */
    bool setSerializeDurationAsNumber(bool val)
    {
        return std::exchange(m_serializeDurationAsNumber, val);
    }

    bool isSerializeDurationAsNumber() const
    {
        return m_serializeDurationAsNumber;
    }

private:
    bool m_serializeDurationAsNumber = false;
};

//-------------------------------------------------------------------------------------------------

// TODO: #akolesnikov Make this block unconditional and use this concept
// template<typename T>
// concept SerializationContext = requires(T a)
// {
//     { decltype(a.composer) } -> std::convertible_to<AbstractComposer>;
// };

template<typename Composer, typename Data> class Visitor;

/**
 * Serializes some type to a format that can be composed using AbstractComposer class.
 * Suitable for json, ubjson, csv, xml.
 */
namespace BasicSerializer {

namespace detail {

template<typename SerializationContext, typename Data>
void serializeAdl(SerializationContext* ctx, const Data& data);

template<typename SerializationContext, typename Data>
void serializeAdl(SerializationContext* ctx, const std::optional<Data>& data);

template<typename T>
inline constexpr bool IsStringAlikeV =
    std::is_convertible_v<T, std::string> ||
    std::is_convertible_v<T, std::string_view> ||
    reflect::detail::HasToBase64V<T> ||
    reflect::detail::HasToStdStringV<T> ||
    reflect::detail::HasToStringV<T>;

template<typename T, bool result>
static void reportUnsupportedType()
{
    static_assert(result, "Type is not serializable");
}

} // namespace detail

template<typename SerializationContext, typename Data>
void serializeAdl(SerializationContext* ctx, const Data& data)
{
    auto state = ctx->beforeSerialize(data);

    // Delegating control to a separate namespace so that current namespace serialize
    // overloads are not considered during the ADL lookup.
    // This is needed to provide a way to override serialization for certain template classes.

    detail::serializeAdl(ctx, data);

    ctx->afterSerialize(data, std::move(state));
}

template<typename Composer>
struct VisitorDeclarator
{
    template<typename... Args> using type = Visitor<Composer, Args...>;
};

template<typename SerializationContext, typename Data>
void serialize(
    [[maybe_unused]] SerializationContext* ctx,
    [[maybe_unused]] const Data& data,
    std::enable_if_t<IsInstrumentedV<Data>>* = nullptr)
{
    typename VisitorDeclarator<SerializationContext>::template type<Data> visitor(ctx, data);

    nx::reflect::visitAllFields<Data>(visitor);
}

template<
    typename SerializationContext,
    typename Container
>
void serialize(
    SerializationContext* ctx,
    const Container& data,
    std::enable_if_t<
        (IsSequenceContainerV<Container> ||
        IsSetContainerV<Container> ||
        IsUnorderedSetContainerV<Container>)
        && !detail::IsStringAlikeV<Container>
    >* = nullptr)
{
    ctx->composer.startArray();
    for (auto it = data.begin(); it != data.end(); ++it)
        serializeAdl(ctx, *it);
    ctx->composer.endArray();
}

template<
    typename SerializationContext,
    typename C
>
void serialize(
    SerializationContext* ctx,
    const C& data,
    std::enable_if_t<
        (IsAssociativeContainerV<C> && !IsSetContainerV<C>)
            || (IsUnorderedAssociativeContainerV<C> && !IsUnorderedSetContainerV<C>)
    >* = nullptr)
{
    ctx->composer.startObject();
    for (auto it = data.begin(); it != data.end(); ++it)
    {
        ctx->composer.writeAttributeName(nx::reflect::toString(it->first));
        serializeAdl(ctx, it->second);
    }
    ctx->composer.endObject();
}

template<typename SerializationContext>
void serialize(
    SerializationContext* ctx,
    const std::string& val)
{
    ctx->composer.writeValue(val);
}

template<typename SerializationContext, typename Value>
void serialize(
    SerializationContext* ctx, const Value& val,
    std::enable_if_t<
        !IsInstrumentedV<Value> &&
        (!IsContainerV<Value> || detail::IsStringAlikeV<Value>)>* = nullptr)
{
    if constexpr (std::is_same_v<Value, bool>)
    {
        ctx->composer.writeValue(val);
    }
    else if constexpr (std::is_integral_v<Value> || std::is_floating_point_v<Value>)
    {
        ctx->composer.writeValue(val);
    }
    else if constexpr (std::is_same_v<Value, std::nullptr_t>)
    {
        ctx->composer.writeValue(val);
    }
    else if constexpr (
        std::is_convertible_v<Value, std::string>
        || std::is_convertible_v<Value, std::string_view>)
    {
        ctx->composer.writeValue(nx::reflect::toString(val));
    }
    else if constexpr (
        useStringConversionForSerialization((const Value*) nullptr)
        || nx::reflect::IsInstrumentedEnumV<Value>)
    {
        ctx->composer.writeValue(nx::reflect::toString(val));
    }
    else
    {
        detail::reportUnsupportedType<Value, false>();
    }
}

template<typename SerializationContext, typename... Args>
void serialize(
    SerializationContext* ctx,
    const std::chrono::duration<Args...>& val)
{
    ctx->composer.writeValue(val);
}

template<typename SerializationContext, typename... Args>
void serialize(
    SerializationContext* ctx,
    const std::chrono::time_point<Args...>& val)
{
    ctx->composer.writeValue(val);
}

template<typename SerializationContext, typename... Args>
void serialize(
    SerializationContext* ctx,
    const std::variant<Args...>& val)
{
    if (val.valueless_by_exception())
        ctx->composer.writeNull();
    else
        std::visit([ctx](auto&& arg) { serializeAdl(ctx, arg); }, val);
}

template<typename SerializationContext, typename T>
void serialize(SerializationContext* ctx, const std::reference_wrapper<T>& val)
{
    serialize(ctx, val.get());
}

namespace detail {

template<typename SerializationContext, typename Data>
void serialize(SerializationContext* ctx, const Data& data)
{
    // SFINAE
    nx::reflect::BasicSerializer::serialize(ctx, data);
}

template<typename SerializationContext, typename Data>
void serializeAdl(SerializationContext* ctx, const Data& data)
{
    // ADL kicks in. A custom serialization function is invoked here.
    serialize(ctx, data);
}

template<typename SerializationContext, typename Data>
void serializeAdl(SerializationContext* ctx, const std::optional<Data>& data)
{
    if (data)
        serializeAdl(ctx, *data);
    else
        ctx->composer.writeNull();
}

} // namespace detail

} // namespace BasicSerializer

//-------------------------------------------------------------------------------------------------

template<typename SerializationContext, typename Data>
class Visitor:
    public nx::reflect::GenericVisitor<Visitor<SerializationContext, Data>>
{
public:
    Visitor(SerializationContext* ctx, const Data& data):
        m_ctx(ctx),
        m_data(data)
    {
        m_ctx->composer.startObject();
    }

    ~Visitor()
    {
        m_ctx->composer.endObject();
    }

    template<typename WrappedField>
    void visitField(const WrappedField& field)
    {
        writeAttribute(field.name(), field.get(m_data));
    }

private:
    SerializationContext* m_ctx = nullptr;
    const Data& m_data;

    template<typename Value>
    void writeAttribute(const char* name, const std::optional<Value>& value)
    {
        if (value)
            writeAttribute(name, *value);
    }

    template<typename Value>
    void writeAttribute(const char* name, const Value& value)
    {
        m_ctx->composer.writeAttributeName(name);
        BasicSerializer::serializeAdl(m_ctx, value);
    }
};

} // namespace nx::reflect
