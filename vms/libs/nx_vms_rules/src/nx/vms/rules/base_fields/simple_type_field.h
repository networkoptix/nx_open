// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../action_field.h"
#include "../event_field.h"

namespace nx::vms::rules {

/**
 * Template field for storing and returning a simple value and getting notification about
 * it changes. Template parameter T is the value type, B is a base class type, D is a derived class
 * that has valueChanged() signal.
 * Should be used as a base type only.
 */
template<class T, class B, class D>
class SimpleTypeField: public B
{
public:
    using value_type = T;

public:
    T value() const { return m_value; };
    void setValue(T value)
    {
        static_assert(std::is_base_of<B, D>());

        if (m_value != value)
        {
            m_value = std::move(value);
            emit static_cast<D*>(this)->valueChanged();
        }
    }

protected:
    SimpleTypeField() = default;
    value_type m_value = {};
};

/**
 * Partial specialization of the SimpleTypeField for the ActionField. Should be used as a base
 * type only.
 */
template<class T, class D>
class SimpleTypeActionField: public SimpleTypeField<T, ActionField, D>
{
    using SimpleTypeField<T, ActionField, D>::m_value;

public:
    virtual QVariant build(const AggregatedEventPtr&) const override
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
template<class T, class D>
class SimpleTypeEventField: public SimpleTypeField<T, EventField, D>
{
    using SimpleTypeField<T, EventField, D>::m_value;

public:
    virtual bool match(const QVariant& value) const override
    {
        return value.value<T>() == m_value;
    };

protected:
    SimpleTypeEventField() = default;
};

} // namespace nx::vms::rules
