#pragma once

#include <string>

#include <nx/sdk/i_attribute.h>

namespace nx {
namespace sdk {
namespace analytics {
namespace common {

class Attribute: public IAttribute
{
public:
    Attribute(
        Type type,
        std::string name,
        std::string value);

    virtual Type type() const override;
    virtual const char* name() const override;
    virtual const char* value() const override;

private:
    Type m_type;
    std::string m_name;
    std::string m_value;
};

} // namespace common
} // namespace analytics
} // namespace sdk
} // namespace nx
