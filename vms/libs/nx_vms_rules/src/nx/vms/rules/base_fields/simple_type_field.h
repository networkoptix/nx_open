// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/utils/openapi_doc.h>

#include "../action_builder_field.h"
#include "../event_filter_field.h"

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

    static QJsonObject openApiDescriptor(const QVariantMap&)
    {
        return utils::getPropertyOpenApiDescriptor(QMetaType::fromType<T>(), true);
    }

protected:
    explicit SimpleTypeField(const FieldDescriptor* descriptor): B{descriptor}
    {
    }

    value_type m_value = {};
};

/**
 * Partial specialization of the SimpleTypeField for the ActionField. Should be used as a base
 * type only.
 */
template<class T, class D>
class SimpleTypeActionField: public SimpleTypeField<T, ActionBuilderField, D>
{
    using SimpleTypeField<T, ActionBuilderField, D>::m_value;

public:
    using SimpleTypeField<T, ActionBuilderField, D>::openApiDescriptor;

    explicit SimpleTypeActionField(const FieldDescriptor* descriptor):
        SimpleTypeField<T, ActionBuilderField, D>{descriptor}
    {
    }

    virtual QVariant build(const AggregatedEventPtr&) const override
    {
        return QVariant::fromValue(m_value);
    };
};

/**
 * Partial specialization of the SimpleTypeField for the EventField. Should be used as a base
 * type only.
 */
template<class T, class D>
class SimpleTypeEventField: public SimpleTypeField<T, EventFilterField, D>
{
    using SimpleTypeField<T, EventFilterField, D>::m_value;

public:
    using SimpleTypeField<T, EventFilterField, D>::openApiDescriptor;
    explicit SimpleTypeEventField(const FieldDescriptor* descriptor):
        SimpleTypeField<T, EventFilterField, D>{descriptor}
    {
    }

    virtual bool match(const QVariant& value) const override
    {
        return value.value<T>() == m_value;
    };
};

} // namespace nx::vms::rules
