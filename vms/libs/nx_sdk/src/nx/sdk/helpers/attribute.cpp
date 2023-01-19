// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "attribute.h"

namespace nx::sdk {

Attribute::Attribute(
    Type type,
    std::string name,
    std::string value,
    float confidence)
    :
    m_type(type),
    m_name(std::move(name)),
    m_value(std::move(value)),
    m_confidence(confidence)
{
}

Attribute::Attribute(std::string name, std::string value, float confidence):
    m_type(IAttribute::Type::undefined),
    m_name(std::move(name)),
    m_value(std::move(value)),
    m_confidence(confidence)
{
}

Attribute::Attribute(const nx::sdk::Ptr<const IAttribute>& data):
    m_type(data->type()),
    m_name(data->name()),
    m_value(data->value()),
    m_confidence(data->confidence())
{
}

IAttribute::Type Attribute::type() const
{
    return m_type;
}

const char* Attribute::name() const
{
    return m_name.c_str();
}

const char* Attribute::value() const
{
    return m_value.c_str();
}

float Attribute::confidence() const
{
    return m_confidence;
}

} // namespace nx::sdk
