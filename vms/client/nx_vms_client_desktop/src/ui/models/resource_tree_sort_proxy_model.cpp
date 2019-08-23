#include "resource_tree_sort_proxy_model.h"

#include <client/client_globals.h>
#include <core/resource/resource.h>

#include <nx/utils/math/fuzzy.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>

QnResourceTreeSortProxyModel::QnResourceTreeSortProxyModel(QObject* parent):
    base_type(parent)
{
}

bool QnResourceTreeSortProxyModel::setData(
    const QModelIndex& index, const QVariant& value, int role)
{
    if (index.column() == Qn::CheckColumn && role == Qt::CheckStateRole)
    {
        // TODO: #vkutin #GDM #common Maybe move these signals to QnResourceTreeModel
        emit beforeRecursiveOperation();
        base_type::setData(index, value, Qt::CheckStateRole);
        emit afterRecursiveOperation();
        return true;
    }

    return base_type::setData(index, value, role);
}

Qt::DropActions QnResourceTreeSortProxyModel::supportedDropActions() const
{
    if (auto model = sourceModel())
        return model->supportedDropActions();

    return {};
}

qreal QnResourceTreeSortProxyModel::nodeOrder(const QModelIndex& index)
{
    using NodeType = nx::vms::client::desktop::ResourceTree::NodeType;

    const auto order = [](NodeType t) { return static_cast<qreal>(t); };

    const auto nodeType = index.data(Qn::NodeTypeRole).value<NodeType>();

    if (nodeType == NodeType::edge)
        return order(NodeType::servers);

    if (nodeType != NodeType::resource)
        return order(nodeType);

    const auto resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
    const bool isVideoWall = resource->hasFlags(Qn::videowall);
    if (isVideoWall)
        return order(NodeType::layoutTours) + 0.5;

    const bool isServer = resource->hasFlags(Qn::server);
    if (isServer)
        return order(NodeType::servers);

    return order(nodeType);
}

bool QnResourceTreeSortProxyModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    const auto leftOrder = nodeOrder(left);
    const auto rightOrder = nodeOrder(right);
    if (!qFuzzyEquals(leftOrder, rightOrder))
        return leftOrder < rightOrder;

    return resourceLessThan(left, right);
}
