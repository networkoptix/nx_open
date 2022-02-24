// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../action_field.h"

namespace nx::vms::rules {

/** Template field for storing and returning a simple value. Should be used as a base type only. */
template<class T>
class SimpleTypeField: public ActionField
{
public:
    using value_type = T;

public:
    virtual QVariant build(const EventData&) const override
    {
        return QVariant::fromValue(m_value);
    };

    T value() const { return m_value; };
    void setValue(T value) { m_value = std::move(value); };

protected:
    SimpleTypeField() = default;

private:
    value_type m_value = {};
};

} // namespace nx::vms::rules
