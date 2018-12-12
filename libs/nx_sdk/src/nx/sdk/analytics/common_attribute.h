#pragma once

#include <string>

#include <nx/sdk/common.h>

namespace nx {
namespace sdk {
namespace analytics {

class CommonAttribute: public IAttribute
{
public:
    CommonAttribute(
        AttributeType attributeType,
        std::string attributeName,
        std::string attributeValue);

    virtual AttributeType type() const override;
    virtual const char* name() const override;
    virtual const char* value() const override;

private:
    AttributeType m_type;
    std::string m_name;
    std::string m_value;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
