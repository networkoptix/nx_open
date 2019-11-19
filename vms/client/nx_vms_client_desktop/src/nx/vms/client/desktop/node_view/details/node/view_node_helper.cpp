#include "view_node_helper.h"

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
    const ViewNodeHelper::ForEachNodeCallback& callback,
    bool onlyLeafNodes)
{
    if (!node || !callback)
        return;

    for (const auto child: node->children())
        forEachNodeInternal(child, callback, onlyLeafNodes);

    if (!onlyLeafNodes || node->isLeaf())
        callback(node);
}

const ViewNodeData* getDataPointer(const QModelIndex& index)
{
    const auto node = ViewNodeHelper::nodeFromIndex(index);
    return node ? &node->data() : nullptr;
}

} // namespace

namespace nx::vms::client::desktop {
namespace node_view {
namespace details {

ViewNodeHelper::ViewNodeHelper(const QModelIndex& index):
    m_data(getDataPointer(index))
{
}

ViewNodeHelper::ViewNodeHelper(const ViewNodeData& data):
    m_data(&data)
{
}

ViewNodeHelper::ViewNodeHelper(const NodePtr& node):
    m_data(node ? &node->data() : nullptr)
{
}

bool ViewNodeHelper::expanded() const
{
    return m_data && m_data->property(isExpandedProperty).toBool();
}

bool ViewNodeHelper::isSeparator() const
{
    return m_data && m_data->property(isSeparatorProperty).toBool();
}

bool ViewNodeHelper::hoverable() const
{
    return !m_data
        || !m_data->hasProperty(hoverableProperty)
        || m_data->property(hoverableProperty).toBool();
}

bool ViewNodeHelper::checkable(int column) const
{
    return m_data && m_data->hasData(column, Qt::CheckStateRole);
}

Qt::CheckState ViewNodeHelper::userCheckedState(int column) const
{
    const auto userCheckRole = makeUserActionRole(Qt::CheckStateRole);
    return m_data && m_data->hasData(column, userCheckRole)
        ? m_data->data(column, userCheckRole).value<Qt::CheckState>()
        : Qt::Unchecked;
}

Qt::CheckState ViewNodeHelper::checkedState(int column, bool isUserAction)
{
    if (!m_data)
        return Qt::Unchecked;

    if (!isUserAction)
        return m_data->data(column, Qt::CheckStateRole).value<Qt::CheckState>();

    const auto userActionCheckRole = makeUserActionRole(Qt::CheckStateRole);
    const bool hasUserCheckStateData = m_data->hasData(column, userActionCheckRole);
    const auto targetCheckRole = makeUserActionRole(Qt::CheckStateRole, hasUserCheckStateData);
    return m_data->data(column, targetCheckRole).value<Qt::CheckState>();
}

qreal ViewNodeHelper::progressValue(int column) const
{
    return m_data && m_data->hasData(column, progressRole)
        ? m_data->data(column, progressRole).toFloat()
        : -1;
}

QString ViewNodeHelper::text(int column) const
{
    return m_data ? m_data->data(column, Qt::DisplayRole).toString() : QString();
}

int ViewNodeHelper::groupSortOrder() const
{
    return m_data ? m_data->property(groupSortOrderProperty).toInt() : 0;
}

bool ViewNodeHelper::useSwitchStyleForCheckbox(int column) const
{
    return m_data && m_data->data(column, useSwitchStyleForCheckboxRole).toBool();
}

int ViewNodeHelper::makeUserActionRole(int initialRole, bool isUserAction)
{
    return isUserAction ? initialRole + userChangesRoleOffset : initialRole;
}

NodePtr ViewNodeHelper::nodeFromIndex(const QModelIndex& index)
{
    if (!index.isValid() || !index.model())
        return NodePtr();

    const auto targetIndex = getLeafIndex(index);
    if (!qobject_cast<const details::NodeViewModel*>(targetIndex.model()))
        return NodePtr();

    return index.isValid()
        ? static_cast<ViewNode*>(targetIndex.internalPointer())->sharedFromThis()
        : NodePtr();
}

void ViewNodeHelper::forEachLeaf(const NodePtr& root, const ViewNodeHelper::ForEachNodeCallback& callback)
{
    forEachNodeInternal(root, callback, true);
}

void ViewNodeHelper::forEachNode(const NodePtr& root, const ViewNodeHelper::ForEachNodeCallback& callback)
{
    forEachNodeInternal(root, callback, false);
}

NodePtr ViewNodeHelper::createSeparatorNode(int groupSortOrder)
{
    return ViewNode::create(ViewNodeDataBuilder().separator().withGroupSortOrder(groupSortOrder));
}

NodePtr ViewNodeHelper::createSimpleNode(
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

NodePtr ViewNodeHelper::createSimpleNode(
    const QString& caption,
    int checkableColumn,
    int groupSortOrder)
{
    return createSimpleNode(caption, NodeList(), checkableColumn, groupSortOrder);
}

} // namespace details
} // namespace node_view
} // namespace nx::vms::client::desktop
