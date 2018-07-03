#include "view_node_helpers.h"

#include <common/common_module.h>
#include <client_core/client_core_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource_access/providers/resource_access_provider.h>
#include <nx/client/core/watchers/user_watcher.h>
#include <nx/client/desktop/resource_views/node_view/nodes/view_node.h>

#include <ui/style/resource_icon_cache.h>

namespace {

using namespace nx::client::desktop;

const auto createCheckableLayoutNode =
    [](const QnResourcePtr& resource) -> NodePtr
    {
        return helpers::createResourceNode(resource, true);
    };

ViewNode::Data getNodeData(
    const QnResourcePtr& resource,
    bool checkable = false,
    Qt::CheckState checkedState = Qt::Unchecked)
{
    ViewNode::Data nodeData;
    nodeData.checkable = checkable;
    nodeData.checkedState = checkedState;

    nodeData.data[ViewNode::NameColumn][Qt::DisplayRole] = resource->getName();
    nodeData.data[ViewNode::NameColumn][Qt::DecorationRole] = qnResIconCache->icon(resource);

    return nodeData;
}

} // namespace

namespace nx {
namespace client {
namespace desktop {
namespace helpers {

NodePtr createParentedLayoutsNode()
{
    const auto commonModule = qnClientCoreModule->commonModule();
    const auto accessProvider = commonModule->resourceAccessProvider();
    const auto userWatcher = commonModule->instance<nx::client::core::UserWatcher>();
    const auto currentUser = userWatcher->user();
    const auto currentUserId = currentUser->getId();

    NodeList childNodes;
    const auto pool = qnClientCoreModule->commonModule()->resourcePool();

    const auto filterUser =
        [accessProvider, currentUser](const QnResourcePtr& resource)
        {
            return accessProvider->hasAccess(currentUser, resource.dynamicCast<QnUserResource>());
        };

    QSet<QnUuid> accessibleUserIds;
    QList<QnUserResourcePtr> accessibleUsers;
    for (const auto& userResource: pool->getResources<QnUserResource>(filterUser))
    {
        accessibleUserIds.insert(userResource->getId());
        accessibleUsers.append(userResource);
    }

    const auto isChildLayout=
        [accessProvider, accessibleUserIds, currentUserId](
            const QnResourcePtr& parent,
            const QnResourcePtr& child)
        {
            const auto user = parent.dynamicCast<QnUserResource>();
            const auto layout = child.dynamicCast<QnLayoutResource>();
            const auto wrongLayout = !layout || layout->flags().testFlag(Qn::local);
            if (wrongLayout || !accessProvider->hasAccess(user, layout))
                return false;

            const auto parentId = layout->getParentId();
            const auto userId = user->getId();
            return userId == parentId
                || (!accessibleUserIds.contains(parentId) && userId == currentUserId);
        };

    for (const auto& userResource: accessibleUsers)
    {
        const auto node = createParentResourceNode(userResource,
            isChildLayout, createCheckableLayoutNode);
        if (node->childrenCount() > 0)
            childNodes.append(node);
    }

    return ViewNode::create(childNodes);
}

NodePtr createResourceNode(
    const QnResourcePtr& resource,
    bool checkable,
    Qt::CheckState checkedState)
{
    return resource ? ViewNode::create(getNodeData(resource, checkable, checkedState)) : NodePtr();
}

NodePtr createParentResourceNode(
    const QnResourcePtr& resource,
    const RelationCheckFunction& relationCheckFunction,
    const NodeCreationFunction& nodeCreationFunction,
    bool checkable,
    Qt::CheckState checkedState)
{
    const auto pool = qnClientCoreModule->commonModule()->resourcePool();

    const NodeCreationFunction creationFunction = nodeCreationFunction
        ? nodeCreationFunction
        : [](const QnResourcePtr& resource) -> NodePtr { return createResourceNode(resource); };

    NodeList childrent;
    for (const auto childResource: pool->getResources())
    {
        if (!relationCheckFunction(resource, childResource))
            continue;

        if (const auto node = creationFunction(childResource))
            childrent.append(node);
    }

    return ViewNode::create(getNodeData(resource), childrent);
}

} // namespace helpers
} // namespace desktop
} // namespace client
} // namespace nx
