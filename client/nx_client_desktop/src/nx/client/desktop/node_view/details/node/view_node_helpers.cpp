#include "view_node_helpers.h"

#include "view_node.h"
#include "view_node_data.h"
#include "view_node_data_builder.h"
#include "view_node_constants.h"
#include "../node_view_model.h"

#include <QtCore/QAbstractProxyModel>

#include <nx/client/core/watchers/user_watcher.h>

namespace {

using namespace nx::client::desktop::node_view::details;

QModelIndex getLeafIndex(const QModelIndex& index)
{
    const auto proxyModel = qobject_cast<const QAbstractProxyModel*>(index.model());
    return proxyModel ? getLeafIndex(proxyModel->mapToSource(index)) : index;
}

void forEachNodeInternal(
    const NodePtr& node,
    const ForEachNodeCallback& callback,
    bool onlyLeafNodes)
{
    if (!node || !callback)
        return;

    for (const auto child: node->children())
        forEachNodeInternal(child, callback, onlyLeafNodes);

    if (!onlyLeafNodes || node->isLeaf())
        callback(node);
}

} // namespace

namespace nx {
namespace client {
namespace desktop {
namespace node_view {
namespace details {

NodePtr nodeFromIndex(const QModelIndex& index)
{
    const auto targetIndex = getLeafIndex(index);
    if (!qobject_cast<const details::NodeViewModel*>(targetIndex.model()))
    {
        NX_ASSERT(false, "Can't deduce index of NodeViewModel!");
        return NodePtr();
    }

    return index.isValid()
        ? static_cast<ViewNode*>(targetIndex.internalPointer())->sharedFromThis()
        : NodePtr();
}

void forEachLeaf(const NodePtr& root, const ForEachNodeCallback& callback)
{
    forEachNodeInternal(root, callback, true);
}

void forEachNode(const NodePtr& root, const ForEachNodeCallback& callback)
{
    forEachNodeInternal(root, callback, false);
}

bool expanded(const NodePtr& node)
{
    return node && expanded(node->nodeData());
}

bool expanded(const ViewNodeData& data)
{
    return data.property(isExpandedProperty).toBool();
}

bool expanded(const QModelIndex& index)
{
    const auto node = nodeFromIndex(index);
    return node && expanded(node->nodeData());
}

bool checkable(const NodePtr& node, int column)
{
    return node && checkable(node->nodeData(), column);
}

bool checkable(const QModelIndex& index)
{
    return !index.data(Qt::CheckStateRole).isNull();
}

bool checkable(const ViewNodeData& data, int column)
{
    return !data.data(column, Qt::CheckStateRole).isNull();
}

bool isSeparator(const ViewNodeData& data)
{
    return data.property(isSeparatorProperty).toBool();
}

bool isSeparator(const NodePtr& node)
{
    return node && isSeparator(node->nodeData());
}

bool isSeparator(const QModelIndex& index)
{
    return isSeparator(nodeFromIndex(index));
}

int siblingGroup(const QModelIndex& index)
{
    const auto node = nodeFromIndex(index);
    return node ? node->property(siblingGroupProperty).toInt() : 0;
}

QString text(const NodePtr& node, int column)
{
    return node ? text(node->nodeData(), column) : QString();
}

QString text(const ViewNodeData& data, int column)
{
    return data.data(column, Qt::DisplayRole).toString();
}

QString text(const QModelIndex& index)
{
    return index.data(Qt::DisplayRole).toString();
}

Qt::CheckState checkedState(const NodePtr& node, int column)
{
    return node ? checkedState(node->nodeData(), column) : Qt::Unchecked;
}

Qt::CheckState checkedState(const ViewNodeData& data, int column)
{
    return data.data(column, Qt::CheckStateRole).value<Qt::CheckState>();
}

Qt::CheckState checkedState(const QModelIndex& index)
{
    return index.data(Qt::CheckStateRole).value<Qt::CheckState>();
}

NodePtr createSeparatorNode(int siblingGroup)
{
    return ViewNode::create(ViewNodeDataBuilder().separator().withSiblingGroup(siblingGroup));
}

NodePtr createSimpleNode(
    const QString& caption,
    const NodeList& children,
    int checkableColumn,
    int siblingGroup)
{
    static const auto kTextDefaultColumn = 0;
    auto data = ViewNodeDataBuilder()
        .withText(kTextDefaultColumn, caption)
        .withSiblingGroup(siblingGroup)
        .data();
    if (checkableColumn != -1)
        ViewNodeDataBuilder(data).withCheckedState(checkableColumn, Qt::Unchecked);

    return ViewNode::create(data, children);
}

NodePtr createSimpleNode(
    const QString& caption,
    int checkableColumn,
    int siblingGroup)
{
    return createSimpleNode(caption, NodeList(), checkableColumn, siblingGroup);
}

} // namespace details
} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx
