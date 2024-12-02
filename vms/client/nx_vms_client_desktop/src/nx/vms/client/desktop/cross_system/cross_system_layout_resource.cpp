// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cross_system_layout_resource.h"

#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_grouping/resource_grouping.h>

#include "cross_system_layout_data.h"

namespace nx::vms::client::desktop {

CrossSystemLayoutResource::CrossSystemLayoutResource()
{
    addFlags(Qn::cross_system);
}

void CrossSystemLayoutResource::update(const CrossSystemLayoutData& layoutData)
{
    if (!NX_ASSERT(layoutData.id == this->getId()))
        return;

    CrossSystemLayoutResourcePtr copy(new CrossSystemLayoutResource());
    fromDataToResource(layoutData, copy);
    copy->setFlags(flags()); //< Do not update current resource flags.
    QnResource::update(copy);
    m_customGroupId = layoutData.customGroupId;
}

QString CrossSystemLayoutResource::getProperty(const QString& key) const
{
    if (key == entity_resource_tree::resource_grouping::kCustomGroupIdPropertyKey)
        return m_customGroupId;

    return base_type::getProperty(key);
}

bool CrossSystemLayoutResource::setProperty(
    const QString& key,
    const QString& value,
    bool markDirty)
{
    if (key == entity_resource_tree::resource_grouping::kCustomGroupIdPropertyKey)
    {
        if (value.toLower() == m_customGroupId.toLower())
            return false;

        const auto previousValue = m_customGroupId;
        m_customGroupId = value;

        emitPropertyChanged(key, previousValue, m_customGroupId);

        return true;
    }

    return base_type::setProperty(key, value, markDirty);
}

LayoutResourcePtr CrossSystemLayoutResource::create() const
{
    return LayoutResourcePtr(new CrossSystemLayoutResource());
}

} // namespace nx::vms::client::desktop
