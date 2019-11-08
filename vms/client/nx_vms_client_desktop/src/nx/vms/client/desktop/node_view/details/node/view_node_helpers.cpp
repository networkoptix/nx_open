#include "view_node_helpers.h"

#include "view_node.h"
#include "view_node_data.h"
#include "view_node_data_builder.h"
#include "view_node_constants.h"
#include "../node_view_model.h"

#include <QtCore/QAbstractProxyModel>

#include <nx/client/core/watchers/user_watcher.h>

namespace {

using namespace nx::vms::client::desktop::node_view::details;

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

namespace nx::vms::client::desktop {
namespace node_view {
namespace details {

int makeUserActionRole(int initialRole, bool isUserAction)
{
    return isUserAction ? initialRole + userChangesRoleOffset : initialRole;
}

NodePtr nodeFromIndex(const QModelIndex& index)
{
    if (!index.isValid() || !index.model())
        return NodePtr();

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
    return node && expanded(node->data());
}

bool expanded(const ViewNodeData& data)
{
    return data.property(isExpandedProperty).toBool();
}

bool expanded(const QModelIndex& index)
{
    const auto node = nodeFromIndex(index);
    return node && expanded(node->data());
}

bool checkable(const NodePtr& node, int column)
{
    return node && checkable(node->data(), column);
}

bool checkable(const QModelIndex& index)
{
    return checkable(nodeFromIndex(index), index.column());
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
    return node && isSeparator(node->data());
}

bool isSeparator(const QModelIndex& index)
{
    return isSeparator(nodeFromIndex(index));
}

qreal progressValue(const NodePtr& node, int column)
{
    return node ? progressValue(node->data(), column) : 0.0;
}

qreal progressValue(const ViewNodeData& data, int column)
{
    return data.data(column, progressRole).toFloat();
}

qreal progressValue(const QModelIndex& index)
{
    return progressValue(nodeFromIndex(index), index.column());
}

bool useSwitchStyleForCheckbox(const NodePtr& node, int column)
{
    return node && useSwitchStyleForCheckbox(node->data(), column);
}

bool useSwitchStyleForCheckbox(const ViewNodeData& data, int column)
{
    return data.data(column, useSwitchStyleForCheckboxRole).toBool();
}

bool useSwitchStyleForCheckbox(const QModelIndex& index)
{
    return useSwitchStyleForCheckbox(nodeFromIndex(index), index.column());
}

int groupSortOrder(const QModelIndex& index)
{
    const auto node = nodeFromIndex(index);
    return node ? node->property(groupSortOrderProperty).toInt() : 0;
}

QString text(const NodePtr& node, int column)
{
    return node ? text(node->data(), column) : QString();
}

QString text(const ViewNodeData& data, int column)
{
    return data.data(column, Qt::DisplayRole).toString();
}

QString text(const QModelIndex& index)
{
    return index.data(Qt::DisplayRole).toString();
}

Qt::CheckState userCheckedState(const NodePtr& node, int column)
{
    return node ? userCheckedState(node->data(), column) : Qt::Unchecked;
}

Qt::CheckState userCheckedState(const ViewNodeData& data, int column)
{
    const auto userCheckRole = makeUserActionRole(Qt::CheckStateRole);
    return data.hasData(column, userCheckRole)
        ? data.data(column, userCheckRole).value<Qt::CheckState>()
        : Qt::Unchecked;
}

Qt::CheckState userCheckedState(const QModelIndex& index)
{
    return userCheckedState(nodeFromIndex(index), index.column());
}

Qt::CheckState checkedState(
    const NodePtr& node,
    int column,
    bool isUserAction)
{
    return node ? checkedState(node->data(), column, isUserAction) : Qt::Unchecked;
}

Qt::CheckState checkedState(
    const ViewNodeData& data,
    int column,
    bool isUserAction)
{
    if (!isUserAction)
        return data.data(column, Qt::CheckStateRole).value<Qt::CheckState>();

    const auto userActionCheckRole = makeUserActionRole(Qt::CheckStateRole);
    const bool hasUserCheckStateData = data.hasData(column, userActionCheckRole);
    const auto targetCheckRole = makeUserActionRole(Qt::CheckStateRole, hasUserCheckStateData);
    return data.data(column, targetCheckRole).value<Qt::CheckState>();
}

Qt::CheckState checkedState(
    const QModelIndex& index,
    bool isUserAction)
{
    return checkedState(nodeFromIndex(index), index.column(), isUserAction);
}

NodePtr createSeparatorNode(int groupSortOrder)
{
    return ViewNode::create(ViewNodeDataBuilder().separator().withGroupSortOrder(groupSortOrder));
}

NodePtr createSimpleNode(
    const QString& caption,
    const NodeList& children,
    int checkableColumn,
    int groupSortOrder)
{
    static const auto kTextDefaultColumn = 0;
    auto data = ViewNodeDataBuilder()
        .withText(kTextDefaultColumn, caption)
        .withGroupSortOrder(groupSortOrder)
        .data();
    if (checkableColumn != -1)
        ViewNodeDataBuilder(data).withCheckedState(checkableColumn, Qt::Unchecked);

    return ViewNode::create(data, children);
}

NodePtr createSimpleNode(
    const QString& caption,
    int checkableColumn,
    int groupSortOrder)
{
    return createSimpleNode(caption, NodeList(), checkableColumn, groupSortOrder);
}

} // namespace details
} // namespace node_view
} // namespace nx::vms::client::desktop
