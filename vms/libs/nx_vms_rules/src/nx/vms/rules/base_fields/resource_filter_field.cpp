// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_filter_field.h"

#include "../field_types.h"

namespace nx::vms::rules {

bool ResourceFilterEventField::match(const QVariant& value) const
{
    return acceptAll()
        ? true
        : ids().contains(value.value<QnUuid>());
}

QVariant ResourceFilterActionField::build(const EventPtr& eventData) const
{
    return QVariant::fromValue(UuidSelection{
        .ids = ids(),
        .all = acceptAll(),
    });
}

bool ResourceFilterFieldBase::acceptAll() const
{
    return m_acceptAll;
}

void ResourceFilterFieldBase::setAcceptAll(bool acceptAll)
{
    m_acceptAll = acceptAll;
}

QSet<QnUuid> ResourceFilterFieldBase::ids() const
{
    return m_ids;
}

void ResourceFilterFieldBase::setIds(const QSet<QnUuid>& ids)
{
    m_ids = ids;
}

} // namespace nx::vms::rules
