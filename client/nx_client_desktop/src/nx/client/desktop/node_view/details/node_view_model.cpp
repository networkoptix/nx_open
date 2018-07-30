#include "node_view_model.h"

#include "node_view_state.h"
#include "node_view_state_patch.h"
#include "node/view_node.h"
#include "node/view_node_fwd.h"
#include "node/view_node_helpers.h"

#include <QtCore/QAbstractProxyModel>

#include <nx/utils/log/log.h>

namespace nx {
namespace client {
namespace desktop {
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
    const auto getAddNodeGuard =
        [this](const PatchItem& item) -> QnRaiiGuardPtr
        {
            const auto path = item.path;
            if (path.isEmpty())
                return QnRaiiGuardPtr();

            const auto parentPath = path.parentPath();
            const auto parentNode = d->state.nodeByPath(parentPath);
            const auto parentIndex = d->getModelIndex(parentNode);
            const int row = path.lastIndex();
            return QnRaiiGuardPtr(new NodeViewModel::ScopedInsertRows(this, parentIndex, row, row));
        };

    const auto getDataChangedGuard =
        [this](const PatchItem& item) -> QnRaiiGuardPtr
        {
            return QnRaiiGuard::createDestructible(
                [this, path = item.path, data = item.data]()
                {
                    const auto node = d->state.rootNode->nodeAt(path);
                    for (const int column: data.usedColumns())
                    {
                        const auto nodeIndex = d->getModelIndex(node, column);
                        emit dataChanged(nodeIndex, nodeIndex, data.rolesForColumn(column));
                    }
                });
        };

    patch.applyTo(std::move(d->state), getAddNodeGuard, getDataChangedGuard);
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
    emit dataChangeOccured(index, value, role);
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
} // namespace desktop
} // namespace client
} // namespace nx

