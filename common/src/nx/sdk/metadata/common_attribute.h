#pragma once

#include <string>

#include <nx/sdk/common.h>

namespace nx {
namespace sdk {
namespace metadata {

class CommonAttribute: public nx::sdk::Attribute
{
public:
    CommonAttribute(
        nx::sdk::AttributeType attributeType,
        const std::string& attributeName,
        const std::string& attributeValue);

    virtual const nx::sdk::AttributeType type() const override;
    virtual const char* name() const override;
    virtual const char* value() const override;

private:
    nx::sdk::AttributeType m_type;
    std::string m_name;
    std::string m_value;
};

} // namespace metadata
} // namespace sdk
} // namespace nx
