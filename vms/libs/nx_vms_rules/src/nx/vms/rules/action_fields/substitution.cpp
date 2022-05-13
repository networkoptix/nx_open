// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "substitution.h"

#include <QtCore/QVariant>

#include "../basic_event.h"

namespace nx::vms::rules {

Substitution::Substitution()
{
}

QVariant Substitution::build(const EventPtr& event) const
{
    // TODO: #mmalofeev what does substitution mean it case of the aggregated event?
    QVariant value;
    if (event)
        value = event->property(m_eventFieldName.toUtf8().data());

    return value.canConvert(QVariant::String)
        ? value.toString() //< TODO: #spanasenko Refactor.
        : QString("{%1}").arg(m_eventFieldName);
}

} // namespace nx::vms::rules
