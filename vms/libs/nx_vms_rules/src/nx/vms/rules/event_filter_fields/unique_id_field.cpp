// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "unique_id_field.h"

namespace nx::vms::rules {

nx::Uuid UniqueIdField::id() const
{
    if (m_id.isNull())
        m_id = nx::Uuid::createUuid();

    return m_id;
}

void UniqueIdField::setId(nx::Uuid id)
{
    m_id = id;
}

bool UniqueIdField::match(const QVariant& eventValue) const
{
    return eventValue.value<nx::Uuid>() == m_id;
}

} // namespace nx::vms::rules
