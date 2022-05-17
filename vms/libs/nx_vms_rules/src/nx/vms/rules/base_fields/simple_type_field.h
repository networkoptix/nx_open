// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../action_field.h"
#include "../event_field.h"

namespace nx::vms::rules {

/** Template field for storing and returning a simple value. Should be used as a base type only. */
template<class T, class B>
class SimpleTypeField: public B
{
public:
    using value_type = T;

public:
    T value() const { return m_value; };
    void setValue(T value) { m_value = std::move(value); };

protected:
    SimpleTypeField() = default;
    value_type m_value = {};
};

/**
 * Partial specialization of the SimpleTypeField for the ActionField. Should be used as a base
 * type only.
 */
template<class T>
class SimpleTypeActionField: public SimpleTypeField<T, ActionField>
{
    using SimpleTypeField<T, ActionField>::m_value;

public:
    virtual QVariant build(const EventPtr&) const override
    {
        return QVariant::fromValue(m_value);
    };

protected:
    SimpleTypeActionField() = default;
};

/**
 * Partial specialization of the SimpleTypeField for the EventField. Should be used as a base
 * type only.
 */
template<class T>
class SimpleTypeEventField: public SimpleTypeField<T, EventField>
{
    using SimpleTypeField<T, EventField>::m_value;

public:
    virtual bool match(const QVariant& value) const override
    {
        return value.value<T>() == m_value;
    };

protected:
    SimpleTypeEventField() = default;
};

} // namespace nx::vms::rules
