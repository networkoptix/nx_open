#include <nx/sdk/analytics/helpers/attribute.h>

namespace nx {
namespace sdk {
namespace analytics {

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

} // namespace analytics
} // namespace sdk
} // namespace nx
