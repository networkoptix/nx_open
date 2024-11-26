// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "serializer.h"

namespace nx::reflect::json::detail {

JsonComposer::JsonComposer():
    m_writer(m_buffer)
{
}

void JsonComposer::startArray()
{
    m_writer.StartArray();
}

void JsonComposer::endArray(int items)
{
    m_writer.EndArray(items);
}

void JsonComposer::startObject()
{
    m_writer.StartObject();
}

void JsonComposer::endObject(int members)
{
    m_writer.EndObject(members);
}

void JsonComposer::writeBool(bool val)
{
    m_writer.Bool(val);
}

void JsonComposer::writeInt(const std::int64_t& val)
{
    m_writer.Int64(val);
}

void JsonComposer::writeFloat(const double& val)
{
    m_writer.Double(val);
}

void JsonComposer::writeString(const std::string_view& val)
{
    m_writer.String(val.data(), (rapidjson::SizeType) val.size());
}

void JsonComposer::writeRawString(const std::string_view& val)
{
    m_writer.RawValue(val.data(), (rapidjson::SizeType) val.size(), (rapidjson::Type::kStringType));
}

void JsonComposer::writeNull()
{
    m_writer.Null();
}

void JsonComposer::writeAttributeName(const std::string_view& name)
{
    m_writer.Key(name.data(), (rapidjson::SizeType) name.size());
}

std::string JsonComposer::take()
{
    return m_buffer.GetString();
}

} // namespace nx::reflect::json::detail
