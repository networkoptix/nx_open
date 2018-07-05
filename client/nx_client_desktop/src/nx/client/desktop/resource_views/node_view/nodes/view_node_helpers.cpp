#include "view_node_helpers.h"

#include <common/common_module.h>
#include <client_core/client_core_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource_access/providers/resource_access_provider.h>
#include <nx/client/core/watchers/user_watcher.h>
#include <nx/client/desktop/resource_views/node_view/nodes/view_node.h>
#include <nx/client/desktop/resource_views/node_view/node_view_constants.h>

#include <ui/style/resource_icon_cache.h>

namespace {

using namespace nx::client::desktop;

const auto createCheckableLayoutNode =
    [](const QnResourcePtr& resource) -> NodePtr
    {
        return helpers::createResourceNode(resource, true);
    };

bool isSelected(const NodePtr& node)
{
    const auto variantState = node->data(node_view::checkMarkColumn, Qt::CheckStateRole);
    return !variantState.isNull() && variantState.value<Qt::CheckState>() == Qt::Checked;
}

ViewNode::Data getResourceNodeData(
    const QnResourcePtr& resource,
    bool checkable = false,
    Qt::CheckState checkedState = Qt::Unchecked)
{
    ViewNode::Data nodeData;
    if (checkable)
        nodeData.data[node_view::checkMarkColumn][Qt::CheckStateRole] = checkedState;

    nodeData.data[node_view::nameColumn][Qt::DisplayRole] = resource->getName();
    nodeData.data[node_view::nameColumn][Qt::DecorationRole] = qnResIconCache->icon(resource);
    nodeData.data[node_view::nameColumn][node_view::resourceRole] = QVariant::fromValue(resource);
    return nodeData;
}

using UserResourceList = QList<QnUserResourcePtr>;
NodePtr createUserLayoutsNode(const UserResourceList& users)
{
    const auto commonModule = qnClientCoreModule->commonModule();
    const auto accessProvider = commonModule->resourceAccessProvider();
    const auto userWatcher = commonModule->instance<nx::client::core::UserWatcher>();
    const auto currentUserId = userWatcher->user()->getId();

    QSet<QnUuid> accessibleUserIds;
    UserResourceList accessibleUsers;
    for (const auto& userResource: users)
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

    NodeList childNodes;
    for (const auto& userResource: accessibleUsers)
    {
        const auto node = helpers::createParentResourceNode(userResource,
            isChildLayout, createCheckableLayoutNode);
        if (node->childrenCount() > 0)
            childNodes.append(node);
    }

    return ViewNode::create(childNodes);
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

    const auto pool = qnClientCoreModule->commonModule()->resourcePool();

    const auto filterUser =
        [accessProvider, currentUser](const QnResourcePtr& resource)
        {
            return accessProvider->hasAccess(currentUser, resource.dynamicCast<QnUserResource>());
        };

    UserResourceList accessibleUsers;
    for (const auto& userResource: pool->getResources<QnUserResource>(filterUser))
        accessibleUsers.append(userResource);

    return createUserLayoutsNode(accessibleUsers);
}

NodePtr createCurrentUserLayoutsNode()
{
    const auto commonModule = qnClientCoreModule->commonModule();
    const auto userWatcher = commonModule->instance<nx::client::core::UserWatcher>();
    const auto currentUser = userWatcher->user();
    const auto root = createUserLayoutsNode({currentUser});
    return root->children().first();
}

NodePtr createResourceNode(
    const QnResourcePtr& resource,
    bool checkable,
    Qt::CheckState checkedState)
{
    return resource
        ? ViewNode::create(getResourceNodeData(resource, checkable, checkedState))
        : NodePtr();
}

NodePtr createParentResourceNode(
    const QnResourcePtr& resource,
    const RelationCheckFunction& relationCheckFunction,
    const NodeCreationFunction& nodeCreationFunction,
    bool checkable, //TODO : use it
    Qt::CheckState checkedState)
{
    const auto pool = qnClientCoreModule->commonModule()->resourcePool();

    const NodeCreationFunction creationFunction = nodeCreationFunction
        ? nodeCreationFunction
        : [](const QnResourcePtr& resource) -> NodePtr { return createResourceNode(resource); };

    NodeList children;
    for (const auto childResource: pool->getResources())
    {
        if (!relationCheckFunction(resource, childResource))
            continue;

        if (const auto node = creationFunction(childResource))
            children.append(node);
    }

    return ViewNode::create(getResourceNodeData(resource), children);
}

QnResourceList getLeafSelectedResources(const NodePtr& node)
{
    if (!node->isLeaf())
    {
        QnResourceList result;
        for (const auto child: node->children())
            result += getLeafSelectedResources(child);
        return result;
    }

    if (!isSelected(node))
        return QnResourceList();

    const auto resource = getResource(node);
    return resource ? QnResourceList({resource}) : QnResourceList();
}

QnResourcePtr getResource(const NodePtr& node)
{
    return node->data(node_view::nameColumn, node_view::resourceRole).value<QnResourcePtr>();
}

} // namespace helpers
} // namespace desktop
} // namespace client
} // namespace nx
