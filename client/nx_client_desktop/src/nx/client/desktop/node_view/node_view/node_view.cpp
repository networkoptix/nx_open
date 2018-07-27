#include "node_view.h"

#include <QtCore/QSortFilterProxyModel>

#include <ui/style/helper.h>
#include <ui/common/indents.h>

namespace {

static const auto kAnyColumn = 0;

QModelIndex getRootModelIndex(
    const QModelIndex& leafIndex,
    const QAbstractItemModel* rootModel)
{
    const auto proxyModel = qobject_cast<const QAbstractProxyModel*>(rootModel);
    if (!proxyModel)
        return rootModel->index(leafIndex.row(), leafIndex.column(), leafIndex.parent());

    const auto sourceIndex = getRootModelIndex(leafIndex, proxyModel->sourceModel());
    return proxyModel->mapFromSource(sourceIndex);
}

} // namespace

namespace nx {
namespace client {
namespace desktop {
namespace node_view {

struct NodeView::Private: public QObject
{
    Private(NodeView* owner, int columnCount);

    void updateModelConnectinos();

    void handleExpanded(const QModelIndex& index);
    void handleCollapsed(const QModelIndex& index);
    void updateExpandedState(const QModelIndex& index);
    void handleRowsInserted(const QModelIndex& parent, int from, int to);
    void handlePatchApplied(const NodeViewStatePatch& patch);

    void handleDataChangedOccured(const QModelIndex& index, const QVariant& value, int role);

    NodeView * const owner;

    details::NodeViewStore store;
    details::NodeViewModel model;
    details::NodeViewGroupSortingModel groupSortingProxyModel;
    details::NodeViewItemDelegate itemDelegate;
};

NodeView::Private::Private(
    NodeView* owner,
    int columnCount)
    :
    owner(owner),
    model(columnCount, &store),
    itemDelegate(owner)//,
{
}

void NodeView::Private::handleExpanded(const QModelIndex& index)
{
    if (const auto node = node_view::helpers::nodeFromIndex(index))
        store.setNodeExpanded(node->path(), true);
    else
        NX_EXPECT(false, "Wrong node!");
}

void NodeView::Private::handleCollapsed(const QModelIndex& index)
{
    if (const auto node = node_view::helpers::nodeFromIndex(index))
        store.setNodeExpanded(node->path(), false);
    else
        NX_EXPECT(false, "Wrong node!");
}

void NodeView::Private::updateExpandedState(const QModelIndex& index)
{
    const bool expanded = node_view::helpers::expanded(index);
    if (owner->isExpanded(index) != expanded)
        owner->setExpanded(index, expanded);
}

void NodeView::Private::handlePatchApplied(const NodeViewStatePatch& patch)
{
    model.applyPatch(patch);

    for (const auto data: patch.changedData)
        updateExpandedState(getRootModelIndex(model.index(data.path, kAnyColumn), owner->model()));
}

void NodeView::Private::handleRowsInserted(const QModelIndex& parent, int from, int to)
{
    // We use model of owner instead of QModelIndex::child because parent index can be invalid.
    for (int row = from; row <= to; ++row)
        updateExpandedState(owner->model()->index(row, kAnyColumn, parent));
}

void NodeView::Private::handleDataChangedOccured(
    const QModelIndex& index,
    const QVariant& value,
    int role)
{
    if (role != Qt::CheckStateRole)
        return;

    const auto node = node_view::helpers::nodeFromIndex(index);
    store.setNodeChecked(node->path(), index.column(), value.value<Qt::CheckState>());
}

//-------------------------------------------------------------------------------------------------

NodeView::NodeView(
    int columnCount,
    QWidget* parent)
    :
    base_type(parent),
    d(new Private(this, columnCount))
{
    d->groupSortingProxyModel.setSourceModel(&d->model);
    setModel(&d->groupSortingProxyModel);
    setSortingEnabled(true);
    setProperty(style::Properties::kSideIndentation, QVariant::fromValue(QnIndents(0, 1)));
    setIndentation(style::Metrics::kDefaultIconSize);
    setItemDelegate(&d->itemDelegate);

    connect(&d->model, &details::NodeViewModel::dataChangeOccured,
        d, &Private::handleDataChangedOccured);

    connect(&d->store, &details::NodeViewStore::patchApplied,
        d, &Private::handlePatchApplied);

    connect(this, &QTreeView::expanded, d, &Private::handleExpanded);
    connect(this, &QTreeView::collapsed, d, &Private::handleCollapsed);
    connect(&d->groupSortingProxyModel, &details::NodeViewModel::rowsInserted,
        d, &Private::handleRowsInserted);
}

NodeView::~NodeView()
{
}

void NodeView::setProxyModel(QSortFilterProxyModel* proxy)
{
    d->groupSortingProxyModel.setSourceModel(proxy);
    proxy->setSourceModel(&d->model);
}

void NodeView::applyPatch(const NodeViewStatePatch& patch)
{
    d->store.applyPatch(patch);
}

void NodeView::setupHeader()
{
}

const details::NodeViewStore& NodeView::store() const
{
    return d->store;
}

details::NodeViewModel& NodeView::sourceModel() const
{
    return d->model;
}

void NodeView::setModel(QAbstractItemModel* model)
{
    NX_EXPECT(false, "NodeView does not allow to explicitly set model!");
}

} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx

