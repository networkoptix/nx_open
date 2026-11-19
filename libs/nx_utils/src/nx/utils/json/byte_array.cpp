// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "byte_array.h"

#include <utility>

#include <rapidjson/writer.h>

namespace nx::json {

QByteArray ByteArrayOutputStream::take()
{
    m_buffer.resize(m_size);
    m_buffer.squeeze(); //< Drop the spare capacity left after the geometric growth.
    m_data = nullptr;
    m_size = 0;

    return std::move(m_buffer);
}

QByteArray toByteArray(const rapidjson::Value& value)
{
    ByteArrayOutputStream stream;
    rapidjson::Writer<ByteArrayOutputStream> writer(stream);
    value.Accept(writer);
    return stream.take();
}

} // namespace nx::json
