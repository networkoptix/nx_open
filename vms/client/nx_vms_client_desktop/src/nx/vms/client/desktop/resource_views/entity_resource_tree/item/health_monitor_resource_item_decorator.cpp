// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "health_monitor_resource_item_decorator.h"

#include <core/resource/resource.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <client/client_globals.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

using NodeType = ResourceTree::NodeType;

HealthMonitorResourceItemDecorator::HealthMonitorResourceItemDecorator(
    entity_item_model::AbstractItemPtr sourceItem)
    :
    base_type(),
    m_sourceItem(std::move(sourceItem))
{
    m_sourceItem->setDataChangedCallback(
        [this](const QVector<int>& roles) { notifyDataChanged(roles); });
}

QVariant HealthMonitorResourceItemDecorator::data(int role) const
{
    if (role == Qn::ResourceIconKeyRole)
    {
        const auto flags = m_sourceItem->data(Qn::ResourceFlagsRole).value<Qn::ResourceFlags>();
        if (flags.testFlag(Qn::server))
        {
            auto iconKey = static_cast<QnResourceIconCache::Key>(m_sourceItem->data(role).toInt());
            iconKey &= !QnResourceIconCache::TypeMask;
            iconKey |= QnResourceIconCache::HealthMonitor;
            return QVariant::fromValue<int>(iconKey);
        }
    }

    return m_sourceItem->data(role);
}

Qt::ItemFlags HealthMonitorResourceItemDecorator::flags() const
{
    return m_sourceItem->flags();
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
