// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "substitution.h"

#include <QtCore/QVariant>

namespace nx::vms::rules {

Substitution::Substitution()
{
}

QVariant Substitution::build(const EventData& eventData) const
{
    const auto& value = eventData.value(m_eventFieldName.toUtf8().data());

    return value.canConvert(QVariant::String)
        ? value.toString()
        : QString("{%1}").arg(m_eventFieldName);
}

} // namespace nx::vms::rules
