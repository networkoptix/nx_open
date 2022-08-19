// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "showreel_item.h"

#include <QtCore/QVariant>

#include <ui/help/help_topics.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <client/client_globals.h>
#include <core/resource_management/layout_tour_manager.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

using namespace nx::vms::api;

ShowreelItem::ShowreelItem(const QnLayoutTourManager* showreelManager, const QnUuid& id):
    base_type()
{
    m_tourData = showreelManager->tour(id);
    m_connectionsGuard.add(
        QObject::connect(showreelManager, &QnLayoutTourManager::tourChanged,
        [this](const LayoutTourData& tour) { onLayoutTourChanged(tour); }));
}

QVariant ShowreelItem::data(int role) const
{
    switch (role)
    {
        case Qt::DisplayRole:
        case Qt::ToolTipRole:
        case Qt::EditRole:
            return m_tourData.name;

        case Qn::ResourceIconKeyRole:
            return QVariant::fromValue<int>(QnResourceIconCache::LayoutTour);

        case Qn::NodeTypeRole:
            return QVariant::fromValue(ResourceTree::NodeType::layoutTour);

        case Qn::UuidRole:
            return QVariant::fromValue(m_tourData.id);

        case Qn::HelpTopicIdRole:
            return QVariant::fromValue<int>(Qn::Showreel_Help);
    }
    return QVariant();
}

Qt::ItemFlags ShowreelItem::flags() const
{
    return {Qt::ItemIsEnabled, Qt::ItemIsSelectable, Qt::ItemIsDragEnabled, Qt::ItemIsEditable,
        Qt::ItemNeverHasChildren};
}

void ShowreelItem::onLayoutTourChanged(const LayoutTourData& tourData)
{
    if (tourData.id != m_tourData.id)
        return;

    const bool nameChanged = tourData.name != m_tourData.name;
    m_tourData = tourData;

    if (nameChanged)
        notifyDataChanged({Qt::DisplayRole, Qt::ToolTipRole, Qt::EditRole});
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
