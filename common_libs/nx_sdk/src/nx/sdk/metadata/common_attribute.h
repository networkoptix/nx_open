#pragma once

#include <string>

#include <nx/sdk/common.h>

namespace nx {
namespace sdk {
namespace metadata {

class CommonAttribute: public Attribute
{
public:
    CommonAttribute(
        AttributeType attributeType,
        const std::string& attributeName,
        const std::string& attributeValue);

    virtual const AttributeType type() const override;
    virtual const char* name() const override;
    virtual const char* value() const override;

private:
    AttributeType m_type;
    std::string m_name;
    std::string m_value;
};

} // namespace metadata
} // namespace sdk
} // namespace nx
