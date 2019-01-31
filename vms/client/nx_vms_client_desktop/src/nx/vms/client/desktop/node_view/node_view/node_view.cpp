#include "node_view.h"

#include "node_view_state_reducer.h"
#include "node_view_item_delegate.h"
#include "sorting/node_view_group_sorting_model.h"
#include "../details/node_view_model.h"
#include "../details/node_view_store.h"
#include "../details/node_view_state.h"
#include "../details/node_view_state_patch.h"
#include "../details/node/view_node.h"
#include "../details/node/view_node_helpers.h"
#include "../details/node/view_node_constants.h"

#include <QtCore/QSortFilterProxyModel>
#include <QtWidgets/QHeaderView>

#include <ui/style/helper.h>
#include <ui/common/indents.h>

namespace {

static const auto kAnyColumn = 0;
static const auto indents = QVariant::fromValue(QnIndents(
    style::Metrics::kDefaultTopLevelMargin,
    style::Metrics::kDefaultTopLevelMargin));

QModelIndex getRootModelIndex(const QModelIndex& leafIndex, const QAbstractItemModel* rootModel)
{
    const auto proxyModel = qobject_cast<const QAbstractProxyModel*>(rootModel);
    if (!proxyModel)
        return rootModel->index(leafIndex.row(), leafIndex.column(), leafIndex.parent());

    const auto sourceIndex = getRootModelIndex(leafIndex, proxyModel->sourceModel());
    return proxyModel->mapFromSource(sourceIndex);
}

using RowFunctor = std::function<void (const QModelIndex& )>;
void iterateRows(
    const QAbstractItemModel& model,
    const QModelIndex& index,
    const RowFunctor& functor)
{
    const int childRowsCount = model.rowCount(index);
    for (int row = 0; row != childRowsCount; ++row)
        iterateRows(model, model.index(row, 0, index), functor);

    functor(index);
}

} // namespace

namespace nx::vms::client::desktop {
namespace node_view {

using namespace details;

struct NodeView::Private: public QObject
{
    Private(NodeView* owner, int columnCount);

    void handleExpanded(const QModelIndex& index);
    void handleCollapsed(const QModelIndex& index);
    void handleExpandStateChanged(const QModelIndex& index, bool expanded);
    void updateExpandedState(const QModelIndex& index);
    void handleRowsInserted(const QModelIndex& parent, int from, int to);
    void handleRowsRemoved(const QModelIndex& parent, int from, int to);
    void handlePatchApplied(const NodeViewStatePatch& patch);
    void tryUpdateHeightToContent();

    NodeView * const owner;
    NodeViewStore store;
    NodeViewModel model;
    NodeViewGroupSortingModel groupSortingProxyModel;
    NodeViewItemDelegate itemDelegate;
    bool heightToContent = true;
};

NodeView::Private::Private(
    NodeView* owner,
    int columnCount)
    :
    owner(owner),
    model(columnCount, &store),
    itemDelegate(owner)
{
    groupSortingProxyModel.setSourceModel(&model);
}

void NodeView::Private::handleExpanded(const QModelIndex& index)
{
    handleExpandStateChanged(index, true);
}

void NodeView::Private::handleCollapsed(const QModelIndex& index)
{
    handleExpandStateChanged(index, false);
}

void NodeView::Private::handleExpandStateChanged(const QModelIndex& index, bool expanded)
{
    if (const auto node = nodeFromIndex(index))
    {
        store.applyPatch(NodeViewStateReducer::setNodeExpandedPatch(
            store.state(), node->path(), expanded));
    }
    else
    {
        NX_ASSERT(false, "Wrong node!");
    }

    tryUpdateHeightToContent();
}

void NodeView::Private::updateExpandedState(const QModelIndex& index)
{
    const bool expandedNode = details::expanded(index);
    if (owner->isExpanded(index) != expandedNode)
        owner->setExpanded(index, expandedNode);
}

void NodeView::Private::handleRowsInserted(const QModelIndex& parent, int from, int to)
{
    // We use model of owner instead of QModelIndex::child because parent index can be invalid.
    for (int row = from; row <= to; ++row)
        updateExpandedState(owner->model()->index(row, kAnyColumn, parent));

    tryUpdateHeightToContent();
}

void NodeView::Private::handleRowsRemoved(const QModelIndex& /*parent*/, int /*from*/, int /*to*/)
{
    tryUpdateHeightToContent();
}


void NodeView::Private::handlePatchApplied(const NodeViewStatePatch& patch)
{
    model.applyPatch(patch);

    for (const auto step: patch.steps)
    {
        if (!step.data.hasProperty(isExpandedProperty))
            continue;

        const auto index = getRootModelIndex(model.index(step.path, kAnyColumn), owner->model());
        updateExpandedState(index);
    }
}

void NodeView::Private::tryUpdateHeightToContent()
{
    if (!heightToContent)
        return;

    const auto header = owner->header();
    int height = header->isVisible() ? header->height() : 0;
    iterateRows(*owner->model(), QModelIndex(),
        [this, &height](const QModelIndex& index) { height += owner->rowHeight(index); });

    owner->setMinimumHeight(height);
}

//-------------------------------------------------------------------------------------------------

NodeView::NodeView(
    int columnCount,
    QWidget* parent)
    :
    base_type(parent),
    d(new Private(this, columnCount))
{
    base_type::setModel(&d->groupSortingProxyModel);

    setSortingEnabled(true);
    setProperty(style::Properties::kSideIndentation, indents);
    setIndentation(style::Metrics::kDefaultIconSize);
    setItemDelegate(&d->itemDelegate);

    connect(&d->model, &details::NodeViewModel::dataChangeRequestOccured,
        this, &NodeView::handleDataChangeRequest);

    connect(&d->store, &details::NodeViewStore::patchApplied,
        d, &Private::handlePatchApplied);

    connect(this, &QTreeView::expanded, d, &Private::handleExpanded);
    connect(this, &QTreeView::collapsed, d, &Private::handleCollapsed);
    connect(&d->groupSortingProxyModel, &details::NodeViewModel::rowsInserted,
        d, &Private::handleRowsInserted);
    connect(&d->groupSortingProxyModel, &details::NodeViewModel::rowsRemoved,
        d, &Private::handleRowsRemoved);
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

const details::NodeViewState& NodeView::state() const
{
    return d->store.state();
}

void NodeView::setHeightToContent(bool value)
{
    if (value == d->heightToContent)
        return;

    d->heightToContent = value;
    d->tryUpdateHeightToContent();
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

void NodeView::handleDataChangeRequest(
    const QModelIndex& index,
    const QVariant& value,
    int role)
{
    if (role != Qt::CheckStateRole)
        return;

    const auto node = nodeFromIndex(index);
    const auto checkedState = value.value<Qt::CheckState>();
    d->store.applyPatch(NodeViewStateReducer::setNodeChecked(
        d->store.state(), node->path(), {index.column()}, checkedState));
}

void NodeView::setModel(QAbstractItemModel* model)
{
    NX_ASSERT(false, "NodeView does not allow to explicitly set model!");
}

} // namespace node_view
} // namespace nx::vms::client::desktop

