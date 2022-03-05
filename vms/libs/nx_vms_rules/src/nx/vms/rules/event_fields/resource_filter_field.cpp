// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_filter_field.h"

#include <QtCore/QVariant>

namespace nx::vms::rules {

ResourceFilterField::ResourceFilterField()
{
}

bool ResourceFilterField::match(const QVariant& value) const
{
    return m_acceptAll
        ? true
        : m_ids.contains(value.toUuid());
}

bool ResourceFilterField::acceptAll() const
{
    return m_acceptAll;
}

void ResourceFilterField::setAcceptAll(bool acceptAll)
{
    m_acceptAll = acceptAll;
}

QSet<QnUuid> ResourceFilterField::ids() const
{
    return m_ids;
}

void ResourceFilterField::setIds(const QSet<QnUuid>& ids)
{
    m_ids = ids;
}

} // namespace nx::vms::rules
