#include "node_view_model.h"

#include "node_view_state.h"
#include "node_view_state_patch.h"
#include "node/view_node.h"
#include "node/view_node_helpers.h"

#include <QtCore/QAbstractProxyModel>

#include <nx/utils/log/log.h>

namespace nx::vms::client::desktop {
namespace node_view {
namespace details {

struct NodeViewModel::Private
{
    Private(NodeViewModel* owner, int columnCount);

    QModelIndex getModelIndex(const NodePtr& node, int column = 0);

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

QModelIndex NodeViewModel::Private::getModelIndex(const NodePtr& node, int column)
{
    const auto parentNode = node ? node->parent() : NodePtr();
    if (!parentNode) //< It is invisible root node.
        return QModelIndex();

    const auto parentIndex = parentNode->parent()
        ? getModelIndex(parentNode, column)
        : QModelIndex(); //< It is top-level node

    return q->index(parentNode->indexOf(node), column, parentIndex);
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
            switch (step.operation)
            {
                case AppendNodeOperation:
                {
                    const auto parentPath = step.path;
                    const auto parentNode = d->state.nodeByPath(parentPath);
                    const auto parentIndex = d->getModelIndex(parentNode);
                    const int row = parentNode->childrenCount();
                    return utils::SharedGuardPtr(
                        new NodeViewModel::ScopedInsertRows(this, parentIndex, row, row));
                }
                case ChangeNodeOperation:
                {
                    return utils::makeSharedGuard(
                        [this, step]()
                        {
                            const auto node = d->state.rootNode->nodeAt(step.path);
                            for (const int column: step.data.usedColumns())
                            {
                                const auto nodeIndex = d->getModelIndex(node, column);
                                emit dataChanged(nodeIndex, nodeIndex,
                                    step.data.rolesForColumn(column));
                            }
                        });
                }
                case RemoveNodeOperation:
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
    return node ? d->getModelIndex(node, column) : QModelIndex();
}

QModelIndex NodeViewModel::index(int row, int column, const QModelIndex& parent) const
{
    if (!rowCount(parent))
        return QModelIndex();

    const auto node = parent.isValid()
        ? nodeFromIndex(parent)->nodeAt(row).data()
        : d->state.rootNode->nodeAt(row).data();

    return createIndex(row, column, node);
}

QModelIndex NodeViewModel::parent(const QModelIndex& child) const
{
    if (!child.isValid())
        QModelIndex();

    const auto node = nodeFromIndex(child);
    const auto parent = node ? node->parent() : NodePtr();
    const bool rootOrFirstLevelNode = !parent || !parent->parent();

    // Parent is always index with first column.
    return rootOrFirstLevelNode ? QModelIndex() : d->getModelIndex(node->parent());
}

int NodeViewModel::rowCount(const QModelIndex& parent) const
{
    const auto node = parent.isValid() ? nodeFromIndex(parent) : d->state.rootNode;
    return node ? node->childrenCount() : 0;
}

int NodeViewModel::columnCount(const QModelIndex& parent) const
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
    const auto node = nodeFromIndex(index);
    return node ? node->data(index.column(), role) : QVariant();
}

Qt::ItemFlags NodeViewModel::flags(const QModelIndex& index) const
{
    const auto node = nodeFromIndex(index);
    return node ? node->flags(index.column()) : base_type::flags(index);
}

} // namespace details
} // node_view
} // namespace nx::vms::client::desktop

