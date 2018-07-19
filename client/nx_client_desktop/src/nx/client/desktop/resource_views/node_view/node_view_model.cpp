#include "node_view_model.h"

#include <nx/utils/log/log.h>

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
    Private(NodeViewModel* owner);

    QModelIndex getModelIndex(const NodePtr& node, int column = node_view::nameColumn);

    NodeViewModel* const q;
    NodeViewState state;
};

NodeViewModel::Private::Private(NodeViewModel* owner):
    q(owner)
{
}

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

NodeViewModel::NodeViewModel(QObject* parent):
    d(new Private(this))
{
}

NodeViewModel::~NodeViewModel()
{
}

void NodeViewModel::applyPatch(const NodeViewStatePatch& patch)
{
    const auto getAddNodeGuard =
        [this](const NodeViewStatePatch::NodeDescription& description) -> QnRaiiGuardPtr
        {
            const auto path = description.path;
            if (path.isEmpty())
                return QnRaiiGuardPtr();

            const auto parentPath = path.parentPath();
            const auto parentNode = d->state.nodeByPath(parentPath);
            const auto parentIndex = d->getModelIndex(parentNode);
            const int row = path.leafIndex();
            return QnRaiiGuardPtr(new NodeViewModel::ScopedInsertRows(this, parentIndex, row, row));
        };

    const auto getDataChangedGuard =
        [this](const NodeViewStatePatch::NodeDescription& description) -> QnRaiiGuardPtr
        {
            return QnRaiiGuard::createDestructible(
                [this, path = description.path, data = description.data]()
                {
                    const auto node = d->state.rootNode->nodeAt(path);
                    for (const int column: data.columns())
                    {
                        const auto nodeIndex = d->getModelIndex(node, column);
                        emit dataChanged(nodeIndex, nodeIndex, data.rolesForColumn(column));
                    }
                });
        };

    applyNodeViewPatch(std::move(d->state), patch, getAddNodeGuard, getDataChangedGuard);
}

QModelIndex NodeViewModel::index(const ViewNodePath& path, int column) const
{
    const auto node = d->state.nodeByPath(path);
    NX_EXPECT(node, "Wrong path!");
    return node ? d->getModelIndex(node, column) : QModelIndex();
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
    if (!child.isValid() || child.column() == node_view::nameColumn)
        QModelIndex();

    const auto node = nodeFromIndex(child);
    return firstLevelOrRootNode(node)
        ? QModelIndex()
        : d->getModelIndex(node->parent());
}

int NodeViewModel::rowCount(const QModelIndex& parent) const
{
    const auto node = parent.isValid() ? nodeFromIndex(parent) : d->state.rootNode;
    return node ? node->childrenCount() : 0;
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
    NX_EXPECT(qobject_cast<const NodeViewModel*>(index.model()), "Model of index is not appropriate!");
    return index.isValid()
        ? static_cast<ViewNode*>(index.internalPointer())->sharedFromThis()
        : NodePtr();
}

} // namespace desktop
} // namespace client
} // namespace nx

