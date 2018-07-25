#include "view_node_helpers.h"

#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>
#include <common/common_module.h>
#include <client_core/client_core_module.h>

#include <nx/client/core/watchers/user_watcher.h>
#include <nx/client/desktop/resource_views/node_view/node/view_node.h>
#include <nx/client/desktop/resource_views/node_view/node/view_node_data.h>
#include <nx/client/desktop/resource_views/node_view/node/view_node_data_builder.h>
#include <nx/client/desktop/resource_views/node_view/details/node_view_model.h>

#include <ui/style/resource_icon_cache.h>

namespace {

using namespace nx::client::desktop;
using namespace nx::client::desktop::helpers;

using UserResourceList = QList<QnUserResourcePtr>;

QModelIndex getLeafIndex(const QModelIndex& index)
{
    const auto proxyModel = qobject_cast<const QAbstractProxyModel*>(index.model());
    return proxyModel ? getLeafIndex(proxyModel->mapToSource(index)) : index;
}

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

NodePtr nodeFromIndex(const QModelIndex& index)
{
    const auto targetIndex = getLeafIndex(index);
    if (!qobject_cast<const details::NodeViewModel*>(targetIndex.model()))
    {
        NX_EXPECT(false, "Can't deduce index of NodeViewModel!");
        return NodePtr();
    }

    return index.isValid()
        ? static_cast<ViewNode*>(targetIndex.internalPointer())->sharedFromThis()
        : NodePtr();
}

QnResourceList getLeafSelectedResources(const NodePtr& node)
{
    if (!node)
        return QnResourceList();

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

bool isAllSiblingsCheckNode(const NodePtr& node)
{
    if (!node)
        return false;

    const auto nodeFlagsValue = node->data(node_view::nameColumn, node_view::nodeFlagsRole);
    const auto nodeFlags = nodeFlagsValue.value<node_view::NodeFlags>();
    return nodeFlags.testFlag(node_view::AllSiblingsCheckFlag);
}

//--

bool hasExpandedData(const ViewNodeData& data)
{
    return !data.genericData(node_view::expandedRole).isNull();
}

bool hasExpandedData(const QModelIndex& index)
{
    const auto node = nodeFromIndex(index);
    return node && !node->genericData(node_view::expandedRole).isNull();
}

bool expanded(const ViewNodeData& data)
{
    const auto expandedData = data.genericData(node_view::expandedRole);
    return !expandedData.isNull() && expandedData.toBool();
}

bool expanded(const QModelIndex& index)
{
    const auto node = nodeFromIndex(index);
    return node && node->genericData(node_view::expandedRole).toBool();
}

bool isCheckable(const NodePtr& node)
{
    return node && !node->data(node_view::checkMarkColumn, Qt::CheckStateRole).isNull();
}

bool isCheckable(const QModelIndex& index)
{
    return isCheckable(nodeFromIndex(index));
}

bool isSeparator(const QModelIndex& index)
{
    const auto node = nodeFromIndex(index);
    return node && node->data(node_view::nameColumn, node_view::separatorRole).toBool();
}

int siblingGroup(const QModelIndex& index)
{
    const auto node = nodeFromIndex(index);
    return node ? node->data(node_view::nameColumn, node_view::siblingGroupRole).toInt() : 0;
}

QString text(const QModelIndex& index)
{
    return index.data(Qt::DisplayRole).toString();
}

QString extraText(const QModelIndex& index)
{
    return index.data(node_view::extraTextRole).toString();
}

Qt::CheckState checkedState(const NodePtr& node)
{
    const auto checkedData = node
        ? node->data(node_view::checkMarkColumn, Qt::CheckStateRole)
        : QVariant();
    return checkedData.isNull() ? Qt::Unchecked : checkedData.value<Qt::CheckState>();
}

Qt::CheckState checkedState(const QModelIndex& index)
{
    return checkedState(nodeFromIndex(index));
}

QnResourcePtr getResource(const NodePtr& node)
{
    return node
        ? node->data(node_view::nameColumn, node_view::resourceRole).value<QnResourcePtr>()
        : QnResourcePtr();
}

QnResourcePtr getResource(const QModelIndex& index)
{
    return getResource(nodeFromIndex(index));
}

} // namespace helpers
} // namespace desktop
} // namespace client
} // namespace nx
