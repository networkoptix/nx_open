// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "main_tree_resource_item_decorator.h"

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <client/client_globals.h>

namespace {

using namespace nx::vms::api;
using namespace nx::vms::client::desktop;

using Permissions =
    nx::vms::client::desktop::entity_resource_tree::MainTreeResourceItemDecorator::Permissions;

bool hasItemIsDragEnabledFlag(const QnResourcePtr& resource)
{
    static constexpr Qn::ResourceFlags kDraggableTypeFlags =
        {Qn::server, Qn::web_page, Qn::layout, Qn::media, Qn::videowall};

    // Any resource node with flags intersecting with combination above is considered
    // as draggable for anyone except fake servers within 'Other Systems' group.
    if (resource->hasFlags(Qn::fake_server))
        return false;

    return (resource->flags() & kDraggableTypeFlags) != 0;
}

bool hasItemIsDropEnabledFlag(const QnResourcePtr& resource, Permissions permissions)
{
    if ((resource->hasFlags(Qn::server) || resource->hasFlags(Qn::user))
        && !resource->hasFlags(Qn::fake))
    {
        return permissions.hasPowerUserPermissions;
    }

    if (resource->hasFlags(Qn::videowall))
        return permissions.permissions.testFlag(Qn::ReadWriteSavePermission);

    return (resource->hasFlags(Qn::layout));
}

bool hasItemIsEditableFlag(
    const QnResourcePtr& resource,
    ResourceTree::NodeType nodeType,
    Permissions permissions)
{
    // Users or servers within 'Other Systems' group cannot be renamed.
    if (resource->hasFlags(Qn::user) || resource->hasFlags(Qn::fake_server))
        return false;

    if (nodeType == ResourceTree::NodeType::layoutItem)
        return false;

    // Any local resources always can be renamed.
    if (resource->hasFlags(Qn::local_image)
        || resource->hasFlags(Qn::local_video)
        || resource->hasFlags(Qn::exported_layout))
    {
        return true;
    }

    if (resource->hasFlags(Qn::layout) && !resource->hasFlags(Qn::exported))
        return permissions.permissions.testFlag(Qn::WriteNamePermission);

    if (resource->hasFlags(Qn::web_page) || resource->hasFlags(Qn::server))
        return permissions.hasPowerUserPermissions;

    if (resource.dynamicCast<QnVirtualCameraResource>()
        || resource->hasFlags(Qn::videowall))
    {
        return permissions.permissions.testFlag(Qn::ReadWriteSavePermission)
            && permissions.permissions.testFlag(Qn::WriteNamePermission);
    }

    return false;
}

} // namespace

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

using NodeType = ResourceTree::NodeType;

MainTreeResourceItemDecorator::MainTreeResourceItemDecorator(
    entity_item_model::AbstractItemPtr sourceItem,
    Permissions permissions,
    NodeType nodeType,
    const std::optional<QnUuid>& itemUuid)
    :
    base_type(),
    m_sourceItem(std::move(sourceItem)),
    m_permissions(permissions),
    m_nodeType(nodeType),
    m_itemUuid(itemUuid)
{
    NX_ASSERT(m_nodeType == NodeType::resource
        || m_nodeType == NodeType::edge
        || m_nodeType == NodeType::sharedResource
        || m_nodeType == NodeType::layoutItem
        || m_nodeType == NodeType::sharedLayout);

    NX_ASSERT((m_nodeType == NodeType::layoutItem) == m_itemUuid.has_value());

    m_sourceItem->setDataChangedCallback(
        [this](const QVector<int>& roles) { notifyDataChanged(roles); });
}

QVariant MainTreeResourceItemDecorator::data(int role) const
{
    if (role == Qn::NodeTypeRole)
        return QVariant::fromValue(m_nodeType);

    if (role == Qn::ItemUuidRole && m_nodeType == NodeType::layoutItem)
        return QVariant::fromValue(*m_itemUuid);

    if (role == Qn::ResourceIconKeyRole
        && (m_nodeType == NodeType::sharedResource || m_nodeType == NodeType::layoutItem))
    {
        const auto flags = m_sourceItem->data(Qn::ResourceFlagsRole).value<Qn::ResourceFlags>();
        if (flags.testFlag(Qn::server) && !flags.testFlag(Qn::fake))
        {
            const auto resource = m_sourceItem->data(Qn::ResourceRole).value<QnResourcePtr>();
            const auto overridenIcon = resource->isOnline()
                ? QnResourceIconCache::HealthMonitor | QnResourceIconCache::Online
                : QnResourceIconCache::HealthMonitor | QnResourceIconCache::Offline;
            return QVariant::fromValue<int>(overridenIcon);
        }
    }

    return m_sourceItem->data(role);
}

Qt::ItemFlags MainTreeResourceItemDecorator::flags() const
{
    auto result = m_sourceItem->flags();

    const auto resource = m_sourceItem->data(Qn::ResourceRole).value<QnResourcePtr>();
    if (!NX_ASSERT(!resource.isNull(), "Resource node expected"))
        return result;

    result.setFlag(Qt::ItemIsEditable, hasItemIsEditableFlag(resource, m_nodeType, m_permissions));
    result.setFlag(Qt::ItemIsDragEnabled, hasItemIsDragEnabledFlag(resource));
    result.setFlag(Qt::ItemIsDropEnabled, hasItemIsDropEnabledFlag(resource, m_permissions));

    return result;
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
