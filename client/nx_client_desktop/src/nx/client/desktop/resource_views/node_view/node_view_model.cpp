#include "node_view_model.h"

#include <nx/client/desktop/resource_views/node_view/node_view_state.h>
#include <nx/client/desktop/resource_views/node_view/node_view_store.h>
#include <nx/client/desktop/resource_views/node_view/node_view_state_patch.h>
#include <nx/client/desktop/resource_views/node_view/node_view_constants.h>
#include <nx/client/desktop/resource_views/node_view/nodes/view_node.h>
#include <nx/client/desktop/resource_views/node_view/nodes/view_node_path.h>

namespace {

using namespace nx::client::desktop;

bool firstLevelOrRootNode(const NodePtr& node)
{
    if (!node)
        return false;

    const auto parent = node->parent();
    return !parent || !parent->parent();
}

} // namespace

namespace nx {
namespace client {
namespace desktop {

struct NodeViewModel::Private
{
    QModelIndex getModelIndex(const NodePtr& node, int column = node_view::nameColumn);

    NodeViewModel* const q;
    NodeViewStore* const store;
};

QModelIndex NodeViewModel::Private::getModelIndex(const NodePtr& node, int column)
{
    const auto parentNode = node ? node->parent() : NodePtr();
    if (!parentNode) //< It is invisible root node.
        return QModelIndex();

    const auto parentIndex = parentNode->parent()
        ? getModelIndex(parentNode)
        : QModelIndex(); //< It is top-level node

    return q->index(parentNode->indexOf(node), column, parentIndex);
}

//-------------------------------------------------------------------------------------------------

NodeViewModel::NodeViewModel(NodeViewStore* store, QObject* parent):
    d(new Private({this, store}))
{
}

NodeViewModel::~NodeViewModel()
{
}

void NodeViewModel::applyPatch(const NodeViewStatePatch& patch)
{
    // TODO: #future uses: add handling of tree structure changes

    for (const auto description: patch.addedNodes)
    {
        const auto path = description.path;
        if (path.isEmpty())
            continue;

        const auto parentPath = path.parentPath();
        const auto parentNode = d->store->state().nodeByPath(parentPath);
        const auto parentIndex = d->getModelIndex(parentNode);
        const int row = path.leafIndex();
        qWarning() << "--- Inserting " << description.data.data[0][Qt::DisplayRole]
            << " to parent " << parentIndex << " with " << row;
        const NodeViewModel::ScopedInsertRows insertGuard(this, parentIndex, row, row);
    }

    for (const auto description: patch.changedData)
    {
        const auto& data = description.data;
        const auto node = d->store->state().rootNode->nodeAt(description.path);
        if (!node)
            continue;

        for (auto itColumnData = data.data.begin(); itColumnData != data.data.end(); ++itColumnData)
        {
            const int column = itColumnData.key();
            const auto nodeIndex = d->getModelIndex(node, column);
            const auto roles = itColumnData.value().keys();
            emit dataChanged(nodeIndex, nodeIndex, roles.toVector());
        }
    }
}

QModelIndex NodeViewModel::index(
    int row,
    int column,
    const QModelIndex& parent) const
{
    if (!rowCount(parent))
        return QModelIndex();

    const auto node = parent.isValid()
        ? nodeFromIndex(parent)->nodeAt(row).data()
        : d->store->state().rootNode->nodeAt(row).data();

    return createIndex(row, column, node);
}

QModelIndex NodeViewModel::parent(const QModelIndex& child) const
{
    if (!child.isValid() || child.column() == node_view::nameColumn)
        QModelIndex();

    const auto node = nodeFromIndex(child);
    return firstLevelOrRootNode(node)
        ? QModelIndex()
        : d->getModelIndex(node->parent());
}

int NodeViewModel::rowCount(const QModelIndex& parent) const
{
    const auto res = [this, parent]()
    {
        const auto node = parent.isValid() ? nodeFromIndex(parent) : d->store->state().rootNode;
        return node ? node->childrenCount() : 0;
    }();
    return res;
}

int NodeViewModel::columnCount(const QModelIndex& parent) const
{
    return node_view::columnCount;
}

bool NodeViewModel::hasChildren(const QModelIndex& parent) const
{
    return rowCount(parent) > 0;
}

bool NodeViewModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    const auto node = nodeFromIndex(index);
    if (!node || role != Qt::CheckStateRole || index.column() != node_view::checkMarkColumn)
        return base_type::setData(index, value, role);

    emit checkedChanged(node->path(), value.value<Qt::CheckState>());
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

NodePtr NodeViewModel::nodeFromIndex(const QModelIndex& index)
{
    return index.isValid()
        ? static_cast<ViewNode*>(index.internalPointer())->sharedFromThis()
        : NodePtr();
}

} // namespace desktop
} // namespace client
} // namespace nx

