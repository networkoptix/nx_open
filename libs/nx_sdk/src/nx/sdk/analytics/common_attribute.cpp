#include "common_attribute.h"

namespace nx {
namespace sdk {
namespace analytics {

CommonAttribute::CommonAttribute(
    AttributeType attributeType,
    std::string attributeName,
    std::string attributeValue)
    :
    m_type(attributeType),
    m_name(std::move(attributeName)),
    m_value(std::move(attributeValue))
{
}

AttributeType CommonAttribute::type() const
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

} // namespace analytics
} // namespace sdk
} // namespace nx
