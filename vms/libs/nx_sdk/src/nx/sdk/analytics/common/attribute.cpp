#include <nx/sdk/analytics/common/attribute.h>

namespace nx {
namespace sdk {
namespace analytics {
namespace common {

Attribute::Attribute(
    Type type,
    std::string name,
    std::string value)
    :
    m_type(type),
    m_name(std::move(name)),
    m_value(std::move(value))
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

} // namespace common
} // namespace analytics
} // namespace sdk
} // namespace nx
