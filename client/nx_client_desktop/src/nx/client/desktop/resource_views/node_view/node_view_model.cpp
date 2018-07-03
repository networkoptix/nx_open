#include "node_view_model.h"

#include <nx/client/desktop/resource_views/node_view/node_view_state.h>
#include <nx/client/desktop/resource_views/node_view/node_view_state_patch.h>.h>
#include <nx/client/desktop/resource_views/node_view/nodes/view_node.h>

namespace {

using namespace nx::client::desktop;

NodePtr nodeFromIndex(const QModelIndex& index)
{
    return index.isValid()
        ? static_cast<ViewNode*>(index.internalPointer())->sharedFromThis()
        : NodePtr();
}

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
    Private(NodeViewModel* owner);

    NodeViewModel* const owner;
    NodeViewState state;

    QModelIndex getModelIndex(const NodePtr& node, int column = ViewNode::NameColumn);
};

NodeViewModel::Private::Private(NodeViewModel* owner):
    owner(owner)
{
}

QModelIndex NodeViewModel::Private::getModelIndex(const NodePtr& node, int column)
{
    const auto parentNode = node->parent();
    if (!parentNode) //< It is invisible root node.
        return QModelIndex();

    const auto parentIndex = parentNode->parent()
        ? getModelIndex(parentNode)
        : QModelIndex(); //< It is top-level node

    return owner->index(parentNode->indexOf(node), column, parentIndex);
}

//-------------------------------------------------------------------------------------------------

NodeViewModel::NodeViewModel(QObject* parent):
    d(new Private(this))
{
}

NodeViewModel::~NodeViewModel()
{
}

const NodeViewState& NodeViewModel::state() const
{
    return d->state;
}

void NodeViewModel::setState(const NodeViewState& state)
{
    ScopedReset reset(this);
    d->state = state;
}

void NodeViewModel::applyPatch(const NodeViewStatePatch& patch)
{
    // TODO: #future uses: add handling of tree structure changes
    d->state = patch.apply(std::move(d->state));
    for (auto it = patch.changedData.begin(); it != patch.changedData.end(); ++it)
    {
        const auto& path = it.key();
        const auto& data = it.value();
        const auto node = d->state.rootNode->nodeAt(path);
        if (!node)
            continue;

        for (auto itColumnData = data.begin(); itColumnData != data.end(); ++itColumnData)
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
        : d->state.rootNode->nodeAt(row).data();

    return createIndex(row, column, node);
}

QModelIndex NodeViewModel::parent(const QModelIndex& child) const
{
    if (!child.isValid() || child.column() == ViewNode::NameColumn)
        QModelIndex();

    const auto node = nodeFromIndex(child);
    return firstLevelOrRootNode(node)
        ? QModelIndex()
        : d->getModelIndex(node->parent());
}

int NodeViewModel::rowCount(const QModelIndex& parent) const
{
    const auto node = parent.isValid() ? nodeFromIndex(parent) : d->state.rootNode;
    return node->childrenCount();
}

int NodeViewModel::columnCount(const QModelIndex& parent) const
{
    return ViewNode::ColumnCount;
}

bool NodeViewModel::hasChildren(const QModelIndex& parent) const
{
    return rowCount(parent) > 0;
}

bool NodeViewModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    const auto node = nodeFromIndex(index);
    if (!node || role != Qt::CheckStateRole || index.column() != ViewNode::CheckMarkColumn)
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

} // namespace desktop
} // namespace client
} // namespace nx

