// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "showreels_list_entity.h"

#include <core/resource/user_resource.h>
#include <nx/vms/api/data/showreel_data.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/item_order/item_order.h>
#include <nx/vms/common/showreel/showreel_manager.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

using namespace nx::vms::api;

ShowreelsListEntity::ShowreelsListEntity(
    const ShowreelItemCreator& showreelItemCreator,
    const common::ShowreelManager* showreelManager,
    const QnUserResourcePtr& user)
    :
    base_type(showreelItemCreator, core::entity_item_model::numericOrder()),
    m_showreelManager(showreelManager),
    m_user(user)
{
    const auto showreels = showreelManager->showreels();

    QVector<nx::Uuid> showreelIds;
    for (const auto& showreel: showreels)
    {
        if (hasAccess(showreel.id))
            showreelIds.push_back(showreel.id);
    }

    setItems(showreelIds);

    m_connectionsGuard.add(QObject::connect(showreelManager,
        &common::ShowreelManager::showreelAdded,
        [this](const ShowreelData& showreel) { onShowreelAdded(showreel); }));

    m_connectionsGuard.add(QObject::connect(showreelManager,
        &common::ShowreelManager::showreelChanged,
        [this](const ShowreelData& showreel) { onShowreelChanged(showreel); }));

    m_connectionsGuard.add(QObject::connect(showreelManager,
        &common::ShowreelManager::showreelRemoved,
        [this](const nx::Uuid& showreelId) { removeItem(showreelId); }));
}

bool ShowreelsListEntity::hasAccess(const nx::Uuid& showreelId) const
{
    if (!m_user || !m_showreelManager)
        return false;

    const auto showreel = m_showreelManager->showreel(showreelId);
    if (!showreel.isValid())
        return false;

    if (showreel.parentId == m_user->getId() || showreel.parentId.isNull())
        return true;

    return false;
}

void ShowreelsListEntity::onShowreelAdded(const ShowreelData& showreel)
{
    if (hasAccess(showreel.id))
        addItem(showreel.id);
}

void ShowreelsListEntity::onShowreelChanged(const ShowreelData& showreel)
{
    const bool hasAccessNow = hasAccess(showreel.id);
    const bool isCurrentlyShown = hasItem(showreel.id);

    if (hasAccessNow && !isCurrentlyShown)
        addItem(showreel.id);
    else if (!hasAccessNow && isCurrentlyShown)
        removeItem(showreel.id);
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
