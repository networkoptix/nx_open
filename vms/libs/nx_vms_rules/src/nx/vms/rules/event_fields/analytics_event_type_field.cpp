// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_event_type_field.h"

#include <QtCore/QVariant>

namespace nx::vms::rules {

bool AnalyticsEventTypeField::match(const QVariant& value) const
{
    const auto typeId = value.value<QnUuid>();

    return typeId == m_typeId;
}

QnUuid AnalyticsEventTypeField::engineId() const
{
    return m_engineId;
}

void AnalyticsEventTypeField::setEngineId(QnUuid id)
{
    m_engineId = id;
}

QnUuid AnalyticsEventTypeField::typeId() const
{
    return m_typeId;
}

void AnalyticsEventTypeField::setTypeId(QnUuid id)
{
    m_typeId = id;
}

} // namespace nx::vms::rules
