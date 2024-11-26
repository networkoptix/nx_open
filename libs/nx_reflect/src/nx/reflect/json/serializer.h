// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <nx/reflect/basic_serializer.h>
#include <nx/reflect/type_utils.h>

#include "json_tags.h"

namespace nx::reflect::json {

namespace detail {

class NX_REFLECT_API JsonComposer:
    public AbstractComposer<std::string>
{
public:
    JsonComposer();

    virtual void startArray() override;
    virtual void endArray(int items) override;
    virtual void startObject() override;
    virtual void endObject(int members) override;
    virtual void writeBool(bool val) override;
    virtual void writeInt(const std::int64_t& val) override;
    virtual void writeFloat(const double& val) override;
    virtual void writeString(const std::string_view& val) override;
    virtual void writeRawString(const std::string_view& val) override;
    virtual void writeNull() override;
    virtual void writeAttributeName(const std::string_view& name) override;

    virtual std::string take() override;

private:
    std::string m_text;
    rapidjson::StringBuffer m_buffer;
    rapidjson::Writer<
        rapidjson::StringBuffer,
        rapidjson::UTF8<>,
        rapidjson::UTF8<>,
        rapidjson::CrtAllocator,
        rapidjson::kWriteNanAndInfNullFlag> m_writer;
};

struct SerializationContext
{
    JsonComposer composer;

    template<typename T>
    unsigned int beforeSerialize(const T&)
    {
        return composer.setSerializeFlags(composer.serializeFlags()
            | (jsonSerializeChronoDurationAsNumber((const T*) nullptr) << 0u)
            | (jsonSerializeInt64AsString((const T*) nullptr) << 1u));
    }

    template<typename T>
    void afterSerialize(const T&, unsigned int state)
    {
        composer.setSerializeFlags(state);
    }
};

} // namespace detail

using SerializationContext = detail::SerializationContext;

template<typename Data>
void serialize(SerializationContext* ctx, const Data& data)
{
    BasicSerializer::serializeAdl(ctx, data);
}

/**
 * Serializes object of supported type Data to JSON.
 * @param data Can be any instrumented type or a container with instrumented types.
 * Member-fields that are not of instrumented types and not of built-in (int, double, std::string) types
 * must be ConvertibleToString for the serialization to work.
 * ConvertibleToString type supports at least one of the following methods:
 * - T::operator std::string() const
 * - std::string T::toStdString() const
 * - ConvertibleToString T::toString() const // returns a different type from T.
 * - ConvertibleToString toString(const T&)  // returns a different type from T.
 * - void toString(const T&, std::string* str)
 */
template<typename Data>
std::string serialize(const Data& data)
{
    SerializationContext ctx;
    nx::reflect::json::serialize(&ctx, data);
    return ctx.composer.take();
}

} // namespace nx::reflect::json
