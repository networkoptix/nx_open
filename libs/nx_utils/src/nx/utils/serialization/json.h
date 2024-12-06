// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <rapidjson/document.h>

#include <nx/reflect/json.h>
#include <nx/utils/log/log_main.h>

namespace nx::utils::serialization::json {

namespace details {

class NX_UTILS_API Composer: public nx::reflect::AbstractComposer<::rapidjson::Document>
{
public:
    Composer()
    {
        setSerializeFlags((/*jsonSerializeChronoDurationAsNumber*/ 1 << 0u)
            | (/*jsonSerializeInt64AsString*/ 1 << 1u));
    }

    virtual void startArray() override { m_value.StartArray(); }
    virtual void endArray(int items) override { m_value.EndArray(items); }
    virtual void startObject() override { m_value.StartObject(); }
    virtual void endObject(int members) override { m_value.EndObject(members); }
    virtual void writeBool(bool value) override { m_value.Bool(value); }
    virtual void writeInt(const std::int64_t& value) override { m_value.Int64(value); }
    virtual void writeFloat(const double& value) override { m_value.Double(value); }
    virtual void writeNull() override { m_value.Null(); }
    virtual void writeString(const std::string_view& value) override
    {
        m_value.String(value.data(), value.size(), /*copy*/ true);
    }

    virtual void writeAttributeName(const std::string_view& name) override
    {
        m_value.Key(name.data(), name.size(), /*copy*/ true);
    }

    virtual void writeRawString(const std::string_view&) override
    {
        NX_ASSERT(false, "Should not get here");
    }

    virtual ::rapidjson::Document take() override
    {
        if (m_value.GetStackCapacity() != 0)
        {
            auto dummy = [](auto&&) { return true; };
            m_value.Populate(dummy);
        }
        return std::move(m_value);
    }

private:
    ::rapidjson::Document m_value;
};

class NX_UTILS_API StripDefaultComposer: public Composer
{
public:
    virtual void startArray() override;
    virtual void endArray(int = 0) override;
    virtual void startObject() override;
    virtual void endObject(int = 0) override;
    virtual void writeBool(bool value) override;
    virtual void writeInt(const std::int64_t& value) override;
    virtual void writeFloat(const double& value) override;
    virtual void writeString(const std::string_view& value) override;
    virtual void writeNull() override;
    virtual void writeAttributeName(const std::string_view& name) override;

private:
    void onWriteValue();

private:
    struct Structured //< Array or object.
    {
        std::string_view firstAttributeWritten; //< Empty for arrays.
        int writtenCount = 0;

        void start(Composer* composer) const
        {
            if (firstAttributeWritten.empty())
            {
                composer->Composer::startArray();
            }
            else
            {
                composer->Composer::startObject();
                composer->Composer::writeAttributeName(firstAttributeWritten);
            }
        }
    };
    std::vector<Structured> m_structured;
};

struct SerializationContextBase
{
    template<typename T>
    unsigned int beforeSerialize(const T&) { return 0u; }

    template<typename T>
    void afterSerialize(const T&, unsigned int) {}
};

} // namespace details

struct SerializationContext: details::SerializationContextBase
{
    details::Composer composer;
};

struct StripDefaultSerializationContext: details::SerializationContextBase
{
    details::StripDefaultComposer composer;
};

template<typename T>
::rapidjson::Document serialized(const T& value, bool stripDefault)
{
    if (!stripDefault)
    {
        SerializationContext context;
        nx::reflect::BasicSerializer::serializeAdl(&context, value);
        return context.composer.take();
    }

    StripDefaultSerializationContext context;
    nx::reflect::BasicSerializer::serializeAdl(&context, value);
    auto result{context.composer.take()};
    if (!result.IsNull())
        return result;

    using namespace nx::reflect;
    if constexpr (IsInstrumentedV<T>
        || IsAssociativeContainerV<T>
        || IsUnorderedAssociativeContainerV<T>)
    {
        result.SetObject();
    }
    else if constexpr (IsArrayV<T>
        || IsSequenceContainerV<T>
        || IsSetContainerV<T>
        || IsUnorderedSetContainerV<T>)
    {
        result.SetArray();
    }
    return result;
}

} // namespace nx::utils::serialization::json

namespace nx::reflect {

template<typename Data>
class Visitor<nx::utils::serialization::json::StripDefaultSerializationContext, Data>:
    public GenericVisitor<
        Visitor<nx::utils::serialization::json::StripDefaultSerializationContext, Data>>
{
public:
    using StripDefaultSerializationContext =
        nx::utils::serialization::json::StripDefaultSerializationContext;

    Visitor(StripDefaultSerializationContext* context, const Data& data):
        m_context(context), m_data(data)
    {
        m_context->composer.startObject();
    }

    ~Visitor() { m_context->composer.endObject(); }

    template<typename WrappedField>
    void visitField(const WrappedField& field)
    {
        const auto& v = field.get(m_data);
        if (!isDefault(field.get(m_default), v))
            writeAttribute(field.name(), v);
    }

private:
    template<typename T, typename = std::void_t<>>
    struct HasEqualOperator: std::false_type {};

    template<typename T>
    struct HasEqualOperator<T, std::void_t<decltype(std::declval<T>() == std::declval<T>())>>:
        std::true_type
    {};

    template<typename T>
    static bool isDefault(const T& lhs, const T& rhs)
    {
        if constexpr (IsOptionalV<T>)
        {
            return !rhs || isDefault(typename T::value_type{}, *rhs);
        }
        else if constexpr (IsVariantV<T>)
        {
            return rhs.valueless_by_exception()
                || std::visit(
                    [](const auto& v) { return isDefault(std::decay_t<decltype(v)>{}, v); }, rhs);
        }
        else if constexpr (IsContainerV<T>)
        {
            NX_ASSERT(lhs.empty());
            return rhs.empty();
        }
        else if constexpr (HasEqualOperator<T>::value)
        {
            return lhs == rhs;
        }
        else if constexpr (IsInstrumentedV<T>)
        {
            return nxReflectVisitAllFields((T*) nullptr,
                [&lhs, &rhs](auto&&... f) { return (isDefault(f.get(lhs), f.get(rhs)) && ...); });
        }
        else
        {
            return nx::reflect::json::serialize(lhs) == nx::reflect::json::serialize(rhs);
        }
    }

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
            m_context->composer.writeAttributeName(name);
            BasicSerializer::serializeAdl(m_context, value);
        }
    }

private:
    StripDefaultSerializationContext* const m_context;
    const Data& m_data;
    const Data m_default{};
};

} // namespace nx::reflect

namespace rapidjson {

template<typename SerializationContext>
void serialize(SerializationContext* context, const Value& value)
{
    if constexpr (std::is_same_v<SerializationContext, nx::reflect::json::SerializationContext>)
    {
        return context->composer.writeRawString(
            nx::reflect::json_detail::getStringRepresentation(value));
    }

    if (value.IsBool())
    {
        context->composer.writeValue(value.GetBool());
    }
    else if (value.IsNumber())
    {
        const auto doubleValue = value.GetDouble();
        const auto intValue = static_cast<int>(doubleValue);
        if (static_cast<double>(intValue) == doubleValue)
            context->composer.writeInt(intValue);
        else
            context->composer.writeFloat(doubleValue);
    }
    else if (value.IsNull())
    {
        context->composer.writeNull();
    }
    else if (value.IsString())
    {
        context->composer.writeValue(std::string_view{value.GetString(), value.GetStringLength()});
    }
    else if (value.IsObject())
    {
        context->composer.startObject();
        for (auto it = value.MemberBegin(); it != value.MemberEnd(); ++it)
        {
            context->composer.writeAttributeName(
                std::string_view{it->name.GetString(), it->name.GetStringLength()});
            serialize(context, it->value);
        }
        context->composer.endObject(value.MemberCount());
    }
    else if (value.IsArray())
    {
        context->composer.startArray();
        for (auto it = value.Begin(); it != value.End(); ++it)
            serialize(context, *it);
        context->composer.endArray(value.Size());
    }
    else
    {
        NX_ASSERT(false, "Unsupported rapidjson::Value type");
    }
}

template<typename SerializationContext>
void serialize(SerializationContext* context, const Document& value)
{
    serialize(context, static_cast<const Value&>(value));
}

} // namespace rapidjson
