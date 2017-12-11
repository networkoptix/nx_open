#include "common_attribute.h"

namespace nx {
namespace sdk {
namespace metadata {

CommonAttribute::CommonAttribute(
    nx::sdk::AttributeType attributeType,
    const std::string& attributeName,
    const std::string& attributeValue)
    :
    m_type(attributeType),
    m_name(attributeName),
    m_value(attributeValue)
{
}

const nx::sdk::AttributeType CommonAttribute::type() const
{
    return m_type;
}

const char* CommonAttribute::name() const
{
    return m_name.c_str();
}

const char* CommonAttribute::value() const
{
    return m_value.c_str();
}

} // namespace metadata
} // namespace sdk
} // namespace nx
