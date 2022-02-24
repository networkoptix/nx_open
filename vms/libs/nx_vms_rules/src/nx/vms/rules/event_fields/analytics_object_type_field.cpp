// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_object_type_field.h"

#include <QtCore/QVariant>

namespace nx::vms::rules {

bool AnalyticsObjectTypeField::match(const QVariant& value) const
{
    const auto typeId = value.value<QnUuid>();
    // TODO: Implement base type matching.
    return typeId == m_typeId;
}

QnUuid AnalyticsObjectTypeField::engineId() const
{
    return m_engineId;
}

void AnalyticsObjectTypeField::setEngineId(QnUuid id)
{
    m_engineId = id;
}

QnUuid AnalyticsObjectTypeField::typeId() const
{
    return m_typeId;
}

void AnalyticsObjectTypeField::setTypeId(QnUuid id)
{
    m_typeId = id;
}

} // namespace nx::vms::rules
