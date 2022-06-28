// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "substitution.h"

#include <QtCore/QVariant>

#include "../basic_event.h"
#include "../event_aggregator.h"

namespace nx::vms::rules {

QVariant Substitution::build(const EventAggregatorPtr& eventAggregator) const
{
    QVariant value;
    if (eventAggregator)
        value = eventAggregator->property(m_fieldName.toUtf8().data());

    return value.canConvert(QVariant::String)
        ? value.toString() //< TODO: #spanasenko Refactor.
        : QString("{%1}").arg(m_fieldName);
}

QString Substitution::fieldName() const
{
    return m_fieldName;
}

void Substitution::setFieldName(const QString& fieldName)
{
    if (m_fieldName != fieldName)
    {
        m_fieldName = fieldName;
        emit fieldNameChanged();
    }
}

} // namespace nx::vms::rules
