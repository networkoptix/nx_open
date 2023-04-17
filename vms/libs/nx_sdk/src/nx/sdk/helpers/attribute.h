// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/i_attribute.h>

namespace nx::sdk {

class Attribute: public nx::sdk::RefCountable<IAttribute>
{
public:
    Attribute(
        Type type,
        std::string name,
        std::string value,
        float confidence = 1.0);

    Attribute(std::string name, std::string value, float confidence = 1.0);

    Attribute(const nx::sdk::Ptr<const IAttribute>& attribute);

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

} // namespace nx::sdk
