// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "substitution.h"

#include <QtCore/QVariant>

#include "../aggregated_event.h"

namespace nx::vms::rules {

Substitution::Substitution()
{
}

QVariant Substitution::build(const AggregatedEvent& aggregatedEvent) const
{
    // TODO: #mmalofeev what does substitution mean it case of the aggregated event?
    QVariant value;
    if (!aggregatedEvent.empty())
        value = aggregatedEvent.initialEvent()->property(m_eventFieldName.toUtf8().data());

    return value.canConvert(QVariant::String)
        ? value.toString() //< TODO: #spanasenko Refactor.
        : QString("{%1}").arg(m_eventFieldName);
}

} // namespace nx::vms::rules
