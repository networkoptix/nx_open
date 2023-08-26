// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "videowall_screen_item.h"

#include <QtCore/QVariant>

#include <client/client_globals.h>
#include <core/resource/videowall_resource.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/common/system_context.h>

namespace {

QnResourceIconCache::Key calculateKey(
    nx::vms::common::SystemContext* context,
    const QnVideoWallItem& item)
{
    QnResourceIconCache::Key result = QnResourceIconCache::VideoWallItem;

    if (!item.runtimeStatus.online)
        return result | QnResourceIconCache::Offline;

    if (item.runtimeStatus.controlledBy.isNull())
        return result;

    if (item.runtimeStatus.controlledBy == context->peerId())
        return result | QnResourceIconCache::Control;

    return result | QnResourceIconCache::Locked;
}

} // namespace

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

VideoWallScreenItem::VideoWallScreenItem(
    const QnVideoWallResourcePtr& videoWall,
    const QnUuid& screenId)
    :
    base_type(),
    m_videoWall(videoWall),
    m_screen(videoWall->items()->getItem(screenId))
{
    m_connectionsGuard.add(
        m_videoWall->connect(m_videoWall.get(), &QnVideoWallResource::itemChanged,
        [this](const QnVideoWallResourcePtr&, const QnVideoWallItem& item,
            const QnVideoWallItem& oldItem)
        {
            onScreenChanged(item, oldItem);
        }));
}

QVariant VideoWallScreenItem::data(int role) const
{
    switch (role)
    {
        case Qt::DisplayRole:
        case Qt::ToolTipRole:
        case Qt::EditRole:
            return m_screen.name;

        case Qn::ResourceIconKeyRole:
            return QVariant::fromValue<int>(calculateKey(m_videoWall->systemContext(), m_screen));

        case Qn::NodeTypeRole:
            return QVariant::fromValue(ResourceTree::NodeType::videoWallItem);

        case Qn::UuidRole:
        case Qn::ItemUuidRole:
            return QVariant::fromValue(m_screen.uuid);

        case Qn::HelpTopicIdRole:
            return QVariant::fromValue<int>(HelpTopic::Id::Videowall_Display);
    }

    return QVariant();
}

Qt::ItemFlags VideoWallScreenItem::flags() const
{
    return {Qt::ItemIsEnabled, Qt::ItemIsSelectable, Qt::ItemIsDragEnabled, Qt::ItemIsDropEnabled,
        Qt::ItemIsEditable, Qt::ItemNeverHasChildren};
}

void VideoWallScreenItem::onScreenChanged(
    const QnVideoWallItem& screen,
    const QnVideoWallItem& oldScreen)
{
    if (m_screen.uuid != screen.uuid)
        return;

    m_screen = screen;

    QVector<int> roles;
    if (screen.name != oldScreen.name)
    {
        roles.push_back(Qt::DisplayRole);
        roles.push_back(Qt::ToolTipRole);
        roles.push_back(Qt::EditRole);
    }

    if ((screen.runtimeStatus.online != oldScreen.runtimeStatus.online)
        || (screen.runtimeStatus.controlledBy != oldScreen.runtimeStatus.controlledBy))
    {
        roles.push_back(Qn::ResourceIconKeyRole);
    }

    if (!roles.empty())
        notifyDataChanged(roles);
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
