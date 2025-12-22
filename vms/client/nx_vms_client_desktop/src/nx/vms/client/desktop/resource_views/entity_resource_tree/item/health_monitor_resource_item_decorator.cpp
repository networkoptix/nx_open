// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "health_monitor_resource_item_decorator.h"

#include <client/client_globals.h>
#include <core/resource/resource.h>
#include <nx/vms/client/core/resource_views/data/resource_tree_globals.h>
#include <nx/vms/client/core/skin/resource_icon_cache.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

using namespace nx::vms::client::core;
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
    if (role == core::ResourceIconKeyRole)
    {
        const auto flags = m_sourceItem->data(core::ResourceFlagsRole).value<Qn::ResourceFlags>();
        if (flags.testFlag(Qn::server))
        {
#if 0
            auto iconKey = static_cast<ResourceIconCache::Key>(m_sourceItem->data(role).toInt());
            iconKey &= ~ResourceIconCache::TypeMask;
            iconKey |= ResourceIconCache::HealthMonitor;
            return QVariant::fromValue<int>(iconKey);
#else
            // Temporary restore old seems invalid behaviour, to check wheter UI tests can pass
            // Line `iconKey &= !ResourceIconCache::TypeMask` always produced zero result before, so this version is equal to the previous one.
            auto iconKey = ResourceIconCache::HealthMonitor;
            return QVariant::fromValue<int>(iconKey);
#endif
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
