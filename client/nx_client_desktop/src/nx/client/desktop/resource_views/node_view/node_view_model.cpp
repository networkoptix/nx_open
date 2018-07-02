#include "node_view_model.h"

#include <nx/client/desktop/resource_views/node_view/node_view_constants.h>
#include <nx/client/desktop/resource_views/node_view/node_view_state.h>
#include <nx/client/desktop/resource_views/node_view/nodes/view_node.h>
#include <nx/client/desktop/resource_views/node_view/node_view_constants.h>

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

    QModelIndex getModelIndex(const NodePtr& node);
};

NodeViewModel::Private::Private(NodeViewModel* owner):
    owner(owner)
{
}

QModelIndex NodeViewModel::Private::getModelIndex(const NodePtr& node)
{
    const auto parentNode = node->parent();
    if (!parentNode) //< It is invisible root node.
        return QModelIndex();

    const auto parentIndex = parentNode->parent()
        ? getModelIndex(parentNode)
        : QModelIndex(); //< It is top-level node

    return owner->index(parentNode->indexOf(node), 0, parentIndex);
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
    if (!child.isValid() || child.column() == NodeViewColumn::Name)
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
    return NodeViewColumn::Count;
}

bool NodeViewModel::hasChildren(const QModelIndex& parent) const
{
    return rowCount(parent) > 0;
}

bool NodeViewModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    const auto node = nodeFromIndex(index);
    if (!node || role != Qt::CheckStateRole || index.column() != NodeViewColumn::CheckMark)
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

