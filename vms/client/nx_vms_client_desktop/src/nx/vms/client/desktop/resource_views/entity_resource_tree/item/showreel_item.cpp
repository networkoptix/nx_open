// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "showreel_item.h"

#include <QtCore/QVariant>

#include <client/client_globals.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/common/showreel/showreel_manager.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

using namespace nx::vms::api;

ShowreelItem::ShowreelItem(const common::ShowreelManager* showreelManager, const QnUuid& id):
    base_type()
{
    m_showreel = showreelManager->showreel(id);
    m_connectionsGuard.add(
        QObject::connect(showreelManager, &common::ShowreelManager::showreelChanged,
        [this](const ShowreelData& tour) { onShowreelChanged(tour); }));
}

QVariant ShowreelItem::data(int role) const
{
    switch (role)
    {
        case Qt::DisplayRole:
        case Qt::ToolTipRole:
        case Qt::EditRole:
            return m_showreel.name;

        case Qn::ResourceIconKeyRole:
            return QVariant::fromValue<int>(QnResourceIconCache::Showreel);

        case Qn::NodeTypeRole:
            return QVariant::fromValue(ResourceTree::NodeType::showreel);

        case Qn::UuidRole:
            return QVariant::fromValue(m_showreel.id);

        case Qn::HelpTopicIdRole:
            return QVariant::fromValue<int>(HelpTopic::Id::Showreel);
    }
    return QVariant();
}

Qt::ItemFlags ShowreelItem::flags() const
{
    return {Qt::ItemIsEnabled, Qt::ItemIsSelectable, Qt::ItemIsDragEnabled, Qt::ItemIsEditable,
        Qt::ItemNeverHasChildren};
}

void ShowreelItem::onShowreelChanged(const ShowreelData& showreel)
{
    if (showreel.id != m_showreel.id)
        return;

    const bool nameChanged = showreel.name != m_showreel.name;
    m_showreel = showreel;

    if (nameChanged)
        notifyDataChanged({Qt::DisplayRole, Qt::ToolTipRole, Qt::EditRole});
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
