#include "view_node_helpers.h"

#include <QtCore/QAbstractProxyModel>

#include <nx/client/core/watchers/user_watcher.h>
#include <nx/client/desktop/resource_views/node_view/node/view_node.h>
#include <nx/client/desktop/resource_views/node_view/node/view_node_data.h>
#include <nx/client/desktop/resource_views/node_view/node/view_node_data_builder.h>
#include <nx/client/desktop/resource_views/node_view/details/node_view_model.h>

namespace {

using namespace nx::client::desktop;
using namespace nx::client::desktop::node_view::helpers;

using UserResourceList = QList<QnUserResourcePtr>;

static constexpr int kDefaultColumn = 0;

QModelIndex getLeafIndex(const QModelIndex& index)
{
    const auto proxyModel = qobject_cast<const QAbstractProxyModel*>(index.model());
    return proxyModel ? getLeafIndex(proxyModel->mapToSource(index)) : index;
}

void setAllSiblingsCheck(ViewNodeData& data)
{
    ViewNodeDataBuilder(data).withAllSiblingsCheckMode();
}

void addSelectAll(const QString& caption, const QIcon& icon, const NodePtr& root)
{
    const auto checkAllNode = createCheckAllNode(caption, icon, -2);
    root->addChild(checkAllNode);
    root->addChild(createSeparatorNode(-1));
}

} // namespace

namespace nx {
namespace client {
namespace desktop {
namespace node_view {
namespace helpers {

NodePtr createSimpleNode(
    const QString& caption,
    const NodeList& children,
    int siblingGroup)
{
    const auto data = ViewNodeDataBuilder()
        .withText(kDefaultColumn, caption)
        .withSiblingGroup(siblingGroup)
        .withCheckedState(1, Qt::Unchecked)//<remove me
        .data();
    return ViewNode::create(data, children);
}

NodePtr createSimpleNode(
    const QString& caption,
    int siblingGroup)
{
    return createSimpleNode(caption, NodeList(), siblingGroup);
}

NodePtr createCheckAllNode(
    const QString& text,
    const QIcon& icon,
    int siblingGroup)
{
    auto data = ViewNodeDataBuilder()
        .withText(kDefaultColumn, text)
        .withIcon(kDefaultColumn, icon)
//        .withCheckedState(Qt::Unchecked)
        .withSiblingGroup(siblingGroup)
        .data();
    setAllSiblingsCheck(data);
    return ViewNode::create(data);
}

NodePtr createSeparatorNode(int siblingGroup)
{
    return ViewNode::create(ViewNodeDataBuilder().separator().withSiblingGroup(siblingGroup));
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

bool isAllSiblingsCheckNode(const NodePtr& node)
{
    return node && node->commonNodeData(node_view::allSiblingsCheckModeCommonRole).toBool();
}

//--


bool expanded(const ViewNodeData& data)
{
    return data.commonNodeData(node_view::expandedCommonRole).toBool();
}

bool expanded(const QModelIndex& index)
{
    const auto node = nodeFromIndex(index);
    return node && expanded(node->nodeData());
}

//bool isCheckable(const NodePtr& node, int column)
//{
//    return node && isCheckable(node->nodeData(), column);
//}

bool isCheckable(const QModelIndex& index)
{
    return !index.data(Qt::CheckStateRole).isNull();
}

bool isCheckable(const ViewNodeData& data, int column)
{
    return !data.data(column, Qt::CheckStateRole).isNull();
}

bool isSeparator(const QModelIndex& index)
{
    const auto node = nodeFromIndex(index);
    return node && node->commonNodeData(node_view::separatorCommonRole).toBool();
}

int siblingGroup(const QModelIndex& index)
{
    const auto node = nodeFromIndex(index);
    return node ? node->commonNodeData(node_view::siblingGroupCommonRole).toInt() : 0;
}

QString text(const QModelIndex& index)
{
    return index.data(Qt::DisplayRole).toString();
}

QString extraText(const QModelIndex& index)
{
    return index.data(node_view::extraTextRole).toString();
}

Qt::CheckState checkedState(const NodePtr& node, int column)
{
    return node ? node->data(column, Qt::CheckStateRole).value<Qt::CheckState>() : Qt::Unchecked;
}

Qt::CheckState checkedState(const QModelIndex& index)
{
    return index.data(Qt::CheckStateRole).value<Qt::CheckState>();
}

} // namespace helpers
} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx
