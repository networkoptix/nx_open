#include "view_node_helpers.h"

#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>
#include <common/common_module.h>
#include <client_core/client_core_module.h>

#include <nx/client/core/watchers/user_watcher.h>
#include <nx/client/desktop/resource_views/node_view/nodes/view_node.h>
#include <nx/client/desktop/resource_views/node_view/nodes/view_node_data.h>
#include <nx/client/desktop/resource_views/node_view/nodes/view_node_data_builder.h>

#include <ui/style/resource_icon_cache.h>

namespace {

using namespace nx::client::desktop;
using namespace nx::client::desktop::helpers;

using UserResourceList = QList<QnUserResourcePtr>;

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

bool isSelected(const NodePtr& node)
{
    const auto variantState = node->data(node_view::checkMarkColumn, Qt::CheckStateRole);
    return !variantState.isNull() && variantState.value<Qt::CheckState>() == Qt::Checked;
}

ViewNodeData getResourceNodeData(
    const QnResourcePtr& resource,
    const OptionalCheckedState& checkedState,
    const ChildrenCountExtraTextGenerator& extraTextGenerator = ChildrenCountExtraTextGenerator(),
    int count = 0)
{
    auto data = ViewNodeDataBuilder()
        .withText(resource->getName())
        .withCheckedState(checkedState)
        .withIcon(qnResIconCache->icon(resource))
        .data();

    data.setData(node_view::nameColumn, node_view::resourceRole, QVariant::fromValue(resource));
    if (count > 0 && extraTextGenerator)
        data.setData(node_view::nameColumn, node_view::extraTextRole, extraTextGenerator(count));
    return data;
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

NodePtr createResourceNode(
    const QnResourcePtr& resource,
    const OptionalCheckedState& checkedState)
{
    return resource
        ? ViewNode::create(getResourceNodeData(resource, checkedState))
        : NodePtr();
}

NodePtr createParentResourceNode(
    const QnResourcePtr& resource,
    const RelationCheckFunction& relationCheckFunction,
    const NodeCreationFunction& nodeCreationFunction,
    const OptionalCheckedState& checkedState,
    const ChildrenCountExtraTextGenerator& extraTextGenerator)
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

    const auto data = getResourceNodeData(resource, checkedState,
        extraTextGenerator, children.count());
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

bool isAllSiblingsCheckNode(const NodePtr& node)
{
    const auto nodeFlagsValue = node->data(node_view::nameColumn, node_view::nodeFlagsRole);
    const auto nodeFlags = nodeFlagsValue.value<node_view::NodeFlags>();
    return nodeFlags.testFlag(node_view::AllSiblingsCheckFlag);
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
