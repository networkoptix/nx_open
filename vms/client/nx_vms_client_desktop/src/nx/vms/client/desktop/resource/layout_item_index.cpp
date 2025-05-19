// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_item_index.h"

#include <nx/utils/log/log.h>
#include <nx/vms/client/core/resource/layout_resource.h>

namespace nx::vms::client::desktop {

LayoutItemIndex::LayoutItemIndex()
{
}

LayoutItemIndex::LayoutItemIndex(const core::LayoutResourcePtr& layout, const nx::Uuid& uuid):
    m_layout(layout),
    m_uuid(uuid)
{
}

const core::LayoutResourcePtr& LayoutItemIndex::layout() const
{
    return m_layout;
}

void LayoutItemIndex::setLayout(const core::LayoutResourcePtr& layout)
{
    m_layout = layout;
}

const nx::Uuid& LayoutItemIndex::uuid() const
{
    return m_uuid;
}

void LayoutItemIndex::setUuid(const nx::Uuid& uuid)
{
    m_uuid = uuid;
}

bool LayoutItemIndex::isNull() const
{
    return m_layout.isNull() || m_uuid.isNull();
}

QString LayoutItemIndex::toString() const
{
    return nx::format("LayoutItemIndex %1 - %2").args(m_layout, m_uuid);
}

} // namespace nx::vms::client::desktop
