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
    virtual void endArray(int items) = 0;

    virtual void startObject() = 0;
    virtual void endObject(int members) = 0;

    virtual void writeBool(bool val) = 0;
    virtual void writeInt(const std::int64_t& val) = 0;
    virtual void writeFloat(const double& val) = 0;
    virtual void writeString(std::string_view val) = 0;
    virtual void writeRawString(std::string_view val) = 0;

    virtual void writeNull() = 0;

    template<typename T>
    void writeValue(T val)
    {
        if constexpr (std::is_same_v<T, bool>)
            writeBool(val);
        else if constexpr (std::is_integral_v<T>)
            sizeof(T) == 8 && int64AsString() ? writeString(std::to_string(val)) : writeInt(val);
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
    void writeValue(const std::chrono::duration<Args...>& val)
    {
        durationAsNumber() ? writeInt(val.count()) : writeString(std::to_string(val.count()));
    }

    template<typename... Args>
    void writeValue(
        const std::chrono::time_point<Args...>& val)
    {
        using namespace std::chrono;
        writeValue(floor<milliseconds>(val.time_since_epoch()));
    }

    virtual void writeAttributeName(std::string_view name) = 0;

    virtual Result take() = 0;

    unsigned int serializeFlags() const { return m_serializeFlags; }
    unsigned int setSerializeFlags(unsigned int value)
    {
        return std::exchange(m_serializeFlags, value);
    }

private:
    bool durationAsNumber() const { return m_serializeFlags & (1u << 0); }
    bool int64AsString() const { return m_serializeFlags & (1u << 1); }

private:
    unsigned int m_serializeFlags = 0;
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

template<
    typename SerializationContext,
    typename Container
>
void serialize(
    SerializationContext* ctx,
    const Container& data,
    std::enable_if_t<IsArrayV<Container>
        || IsSequenceContainerV<Container>
        || IsSetContainerV<Container>
        || IsUnorderedSetContainerV<Container>
    >* = nullptr)
{
    ctx->composer.startArray();
    for (auto it = data.begin(); it != data.end(); ++it)
        serializeAdl(ctx, *it);
    ctx->composer.endArray(data.size());
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
    constexpr bool hasSpecificToStringForKey =
        requires(const SerializationContext& c, const typename C::key_type& v) { c.toString(v); };

    ctx->composer.startObject();
    for (auto it = data.begin(); it != data.end(); ++it)
    {
        if constexpr (hasSpecificToStringForKey)
            ctx->composer.writeAttributeName(ctx->toString(it->first));
        else
            ctx->composer.writeAttributeName(nx::reflect::toString(it->first));
        serializeAdl(ctx, it->second);
    }
    ctx->composer.endObject(data.size());
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
    SerializationContext* ctx, const Value& val, std::enable_if_t<!IsContainerV<Value>>* = nullptr)
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
    else if constexpr (IsStringAlikeV<Value> || IsInstrumentedEnumV<Value>)
    {
        ctx->composer.writeValue(nx::reflect::toString(val));
    }
    else if constexpr (IsInstrumentedV<Value>)
    {
        typename VisitorDeclarator<SerializationContext>::template type<Value> visitor(ctx, val);
        nx::reflect::visitAllFields<Value>(visitor);
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

    ~Visitor() { m_ctx->composer.endObject(m_attributes); }

    template<typename WrappedField>
    void visitField(const WrappedField& field)
    {
        writeAttribute(field.name(), field.get(m_data));
    }

private:
    SerializationContext* m_ctx = nullptr;
    const Data& m_data;
    int m_attributes = 0;

    template<typename Value>
    void writeAttribute(const char* name, const std::optional<Value>& value)
    {
        if (value)
            writeAttribute(name, *value);
    }

    template<typename Value>
    void writeAttribute(const char* name, const Value& value)
    {
        if constexpr (std::is_pointer_v<Value>)
        {
            if (value)
                writeAttribute(name, *value);
        }
        else
        {
            m_ctx->composer.writeAttributeName(name);
            BasicSerializer::serializeAdl(m_ctx, value);
            ++m_attributes;
        }
    }
};

} // namespace nx::reflect
