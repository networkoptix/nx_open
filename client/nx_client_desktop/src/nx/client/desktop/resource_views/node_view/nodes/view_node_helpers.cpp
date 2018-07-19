#include "view_node_helpers.h"

#include <common/common_module.h>
#include <client_core/client_core_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource_access/providers/resource_access_provider.h>
#include <nx/client/core/watchers/user_watcher.h>
#include <nx/client/desktop/resource_views/node_view/nodes/view_node.h>
#include <nx/client/desktop/resource_views/node_view/nodes/view_node_data.h>
#include <nx/client/desktop/resource_views/node_view/nodes/view_node_data_builder.h>

#include <ui/style/resource_icon_cache.h>

namespace {

using namespace nx::client::desktop;

void setAllSiblingsCheck(ViewNodeData& data)
{
    const auto nodeFlags = data.data(node_view::nameColumn, node_view::nodeFlagsRole)
        .value<node_view::NodeFlags>();

    data.setData(node_view::nameColumn, node_view::nodeFlagsRole,
        QVariant::fromValue(nodeFlags | node_view::AllSiblingsCheckFlag));
}

void addSelectAll(const QString& caption, const QIcon& icon, const NodePtr& root)
{
    const auto checkAllNode = helpers::createCheckAllNode(caption, icon, -2);
    root->addChild(checkAllNode);
    root->addChild(helpers::createSeparatorNode(-1));
}

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

ViewNodeData getResourceNodeData(
    const QnResourcePtr& resource,
    bool checkable = false,
    Qt::CheckState checkedState = Qt::Unchecked,
    const QString& parentExtraTextTemplate = QString(),
    int count = 0)
{
    auto data = ViewNodeDataBuilder()
        .withText(resource->getName())
        .withCheckable(checkable, checkedState)
        .withIcon(qnResIconCache->icon(resource))
        .data();

    data.setData(node_view::nameColumn, node_view::resourceRole, QVariant::fromValue(resource));
    if (count > 0 && !parentExtraTextTemplate.isEmpty())
        data.setData(node_view::nameColumn, node_view::extraTextRole, parentExtraTextTemplate.arg(count));
    return data;
}

using UserResourceList = QList<QnUserResourcePtr>;
NodePtr createUserLayoutsNode(
    const UserResourceList& users,
    const QString& extraTextTemplate)
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
            isChildLayout, createCheckableLayoutNode, true, Qt::Unchecked, extraTextTemplate);
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

NodePtr createNode(
    const QString& caption,
    const NodeList& children,
    int siblingGroup)
{
    const auto data = ViewNodeDataBuilder()
        .withText(caption)
        .withSiblingGroup(siblingGroup)
        .withCheckedState(Qt::Unchecked)//-------
        .data();
    return ViewNode::create(data, children);
}

NodePtr createNode(
    const QString& caption,
    int siblingGroup)
{
    return createNode(caption, NodeList(), siblingGroup);
}

NodePtr createCheckAllNode(
    const QString& text,
    const QIcon& icon,
    int siblingGroup)
{
    auto data = ViewNodeDataBuilder()
        .withText(text)
        .withIcon(icon)
        .withCheckedState(Qt::Unchecked)
        .withSiblingGroup(siblingGroup)
        .data();
    setAllSiblingsCheck(data);
    return ViewNode::create(data);
}

NodePtr createSeparatorNode(int siblingGroup)
{
    return ViewNode::create(ViewNodeDataBuilder().separator().withSiblingGroup(siblingGroup));
}

NodePtr createParentedLayoutsNode(bool allowSelectAll, const QString& extraTextTemplate)
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

    const auto root = createUserLayoutsNode(accessibleUsers, extraTextTemplate);
    if (allowSelectAll)
        addSelectAll(lit("Select All"), QIcon(), root);

    return root;
}

NodePtr createCurrentUserLayoutsNode(bool allowSelectAll)
{
    const auto commonModule = qnClientCoreModule->commonModule();
    const auto userWatcher = commonModule->instance<nx::client::core::UserWatcher>();
    const auto currentUser = userWatcher->user();
    const auto root = createUserLayoutsNode({currentUser}, QString());
    const auto userRoot = root->children().first();
    if (allowSelectAll)
        addSelectAll(lit("Select All"), QIcon(), userRoot);
    return userRoot;
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
    bool checkable,
    Qt::CheckState checkedState,
    const QString& parentExtraTextTemplate)
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

    const auto data = getResourceNodeData(resource, checkable, Qt::Unchecked,
        parentExtraTextTemplate, children.count());
    return ViewNode::create(data, children);
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
    if (node)
        return node->data(node_view::nameColumn, node_view::resourceRole).value<QnResourcePtr>();
    else
        NX_EXPECT(false, "Node is null");

    return QnResourcePtr();
}

bool checkableNode(const NodePtr& node)
{
    return !node->data(node_view::checkMarkColumn, Qt::CheckStateRole).isNull();
}

Qt::CheckState nodeCheckedState(const NodePtr& node)
{
    const auto checkedData = node->data(node_view::checkMarkColumn, Qt::CheckStateRole);
    return checkedData.isNull() ? Qt::Unchecked : checkedData.value<Qt::CheckState>();
}

QString nodeText(const NodePtr& node, int column)
{
    return node->data(column, Qt::DisplayRole).toString();
}

QString nodeExtraText(const NodePtr& node, int column)
{
    return node->data(column, node_view::extraTextRole).toString();
}

} // namespace helpers
} // namespace desktop
} // namespace client
} // namespace nx
