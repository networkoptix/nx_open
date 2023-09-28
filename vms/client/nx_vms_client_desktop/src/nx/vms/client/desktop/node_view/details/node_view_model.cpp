// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "node_view_model.h"

#include "node_view_state.h"
#include "node_view_state_patch.h"
#include "node/view_node.h"
#include "node/view_node_helper.h"
#include "node/view_node_constants.h"

#include <QtCore/QAbstractProxyModel>

#include <nx/utils/log/log.h>

namespace nx::vms::client::desktop {
namespace node_view {
namespace details {

struct NodeViewModel::Private
{
    Private(NodeViewModel* owner, int columnCount);

    NodeViewModel* const q;
    const int columnCount;
    NodeViewState state;
};

NodeViewModel::Private::Private(
    NodeViewModel* owner,
    int columnCount)
    :
    q(owner),
    columnCount(columnCount)
{
}

//-------------------------------------------------------------------------------------------------

NodeViewModel::NodeViewModel(
    int columnCount,
    QObject* parent)
    :
    d(new Private(this, columnCount))
{
}

NodeViewModel::~NodeViewModel()
{
}

void NodeViewModel::applyPatch(const NodeViewStatePatch& patch)
{
    const auto getNodeOperationGuard =
        [this](const PatchStep& step) -> utils::SharedGuardPtr
        {
            switch (step.operationData.operation)
            {
                case appendNodeOperation:
                {
                    const auto parentPath = step.path;
                    const auto parentNode = d->state.nodeByPath(parentPath);
                    const auto parentIndex = index(parentNode);
                    const int row = parentNode->childrenCount();
                    return utils::SharedGuardPtr(
                        new NodeViewModel::ScopedInsertRows(this, parentIndex, row, row));
                }
                case updateDataOperation:
                case removeDataOperation:
                {
                    const auto rolesHash =
                        [step]()
                        {
                            if (step.operationData.operation == removeDataOperation)
                                return step.operationData.data.value<ColumnRoleHash>();

                            const auto& data = step.operationData.data.value<ViewNodeData>();
                            ColumnRoleHash result;
                            for (const auto column: data.usedColumns())
                                result.insert(column, data.rolesForColumn(column));

                            return result;
                        }();

                    return utils::makeSharedGuard(
                        [this, path = step.path, rolesHash]()
                        {
                            const auto node = d->state.rootNode->nodeAt(path);
                            for (const auto column: rolesHash.keys())
                            {
                                const auto nodeIndex = index(node, column);
                                emit dataChanged(nodeIndex, nodeIndex, rolesHash.value(column));
                            }
                        });
                }
                case removeNodeOperation:
                {
                    const auto node = d->state.nodeByPath(step.path);
                    const auto parent = node->parent();
                    const int row = parent ? parent->indexOf(node) : 0;
                    const auto parentIndex = index(step.path.parentPath(), 0);
                    return utils::SharedGuardPtr(new NodeViewModel::ScopedRemoveRows(
                        this, parentIndex, row, row));
                }
                default:
                {
                    NX_ASSERT(false, "Operation is not supported");
                    return utils::SharedGuardPtr();
                }
            }
        };

    d->state = patch.applyTo(std::move(d->state), getNodeOperationGuard);
}

QModelIndex NodeViewModel::index(const ViewNodePath& path, int column) const
{
    const auto node = d->state.nodeByPath(path);
    return node ? index(node, column) : QModelIndex();
}

QModelIndex NodeViewModel::index(int row, int column, const QModelIndex& parent) const
{
    if (!rowCount(parent))
        return QModelIndex();

    const auto node = parent.isValid()
        ? ViewNodeHelper::nodeFromIndex(parent)->nodeAt(row).data()
        : d->state.rootNode->nodeAt(row).data();

    return createIndex(row, column, node);
}

QModelIndex NodeViewModel::parent(const QModelIndex& child) const
{
    if (!child.isValid())
        QModelIndex();

    const auto node = ViewNodeHelper::nodeFromIndex(child);
    const auto parent = node ? node->parent() : NodePtr();
    const bool rootOrFirstLevelNode = !parent || !parent->parent();

    // Parent is always index with first column.
    return rootOrFirstLevelNode ? QModelIndex() : index(node->parent());
}

int NodeViewModel::rowCount(const QModelIndex& parent) const
{
    const auto node = parent.isValid() ? ViewNodeHelper::nodeFromIndex(parent) : d->state.rootNode;
    return node ? node->childrenCount() : 0;
}

int NodeViewModel::columnCount(const QModelIndex& /*parent*/) const
{
    return d->columnCount;
}

bool NodeViewModel::hasChildren(const QModelIndex& parent) const
{
    return rowCount(parent) > 0;
}

bool NodeViewModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    emit dataChangeRequestOccured(index, value, role);
    return true;
}

QVariant NodeViewModel::data(const QModelIndex& index, int role) const
{
    const auto node = ViewNodeHelper::nodeFromIndex(index);
    if (!node)
        return QVariant();

    const int column = index.column();
    if (role != Qt::CheckStateRole)
        return node->data(column, role);

    return ViewNodeHelper(index).checkable(column)
        ? ViewNodeHelper(index).checkedState(column, true)
        : QVariant();
}

Qt::ItemFlags NodeViewModel::flags(const QModelIndex& index) const
{
    const auto node = ViewNodeHelper::nodeFromIndex(index);
    return node ? node->flags(index.column()) : base_type::flags(index);
}

QModelIndex NodeViewModel::index(const NodePtr& node, int column) const
{
    const auto parentNode = node ? node->parent() : NodePtr();
    if (!parentNode) //< It is invisible root node.
        return QModelIndex();

    const auto parentIndex = parentNode->parent()
        ? index(parentNode, column)
        : QModelIndex(); //< It is top-level node

    return index(parentNode->indexOf(node), column, parentIndex);
}


} // namespace details
} // node_view
} // namespace nx::vms::client::desktop
