#pragma once

#include <string>

#include <nx/sdk/i_attribute.h>
#include <nx/sdk/helpers/ref_countable.h>

namespace nx {
namespace sdk {

class Attribute: public nx::sdk::RefCountable<IAttribute>
{
public:
    Attribute(
        Type type,
        std::string name,
        std::string value,
        float confidence = 1.0);

    virtual Type type() const override;
    virtual const char* name() const override;
    virtual const char* value() const override;
    virtual float confidence() const override;

    void setValue(std::string value) { m_value = std::move(value); }

private:
    const Type m_type;
    const std::string m_name;
    std::string m_value;
    float m_confidence;
};

} // namespace sdk
} // namespace nx
