#pragma once

#include <string>

#include <nx/sdk/i_attribute.h>

namespace nx {
namespace sdk {
namespace analytics {

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

    void setValue(std::string value) { m_value = std::move(value); }

private:
    const Type m_type;
    const std::string m_name;
    std::string m_value;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
