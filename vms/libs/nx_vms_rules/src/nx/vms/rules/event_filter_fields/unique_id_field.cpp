// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "unique_id_field.h"

namespace nx::vms::rules {

QnUuid UniqueIdField::id() const
{
    if (m_id.isNull())
        m_id = QnUuid::createUuid();

    return m_id;
}

void UniqueIdField::setId(QnUuid id)
{
    m_id = id;
}

bool UniqueIdField::match(const QVariant& eventValue) const
{
    return eventValue.value<QnUuid>() == m_id;
}

} // namespace nx::vms::rules
