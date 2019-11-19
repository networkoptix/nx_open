#include "tree_node_view.h"

#include "node_view_item_delegate.h"
#include "../node_view/node_view_state_reducer.h"
#include "../node_view/sorting_filtering/node_view_group_sorting_model.h"
#include "../details/node_view_model.h"
#include "../details/node_view_store.h"
#include "../details/node_view_state.h"
#include "../details/node_view_state_patch.h"
#include "../details/node/view_node.h"
#include "../details/node/view_node_helper.h"
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

struct TreeNodeView::Private: public QObject
{
    Private(TreeNodeView* owner, int columnCount);

    void handleExpanded(const QModelIndex& index);
    void handleCollapsed(const QModelIndex& index);
    void handleExpandStateChanged(const QModelIndex& index, bool expanded);
    void updateExpandedState(const QModelIndex& index);
    void handleRowsInserted(const QModelIndex& parent, int from, int to);
    void handlePatchApplied(const NodeViewStatePatch& patch);

    TreeNodeView* const owner;
    NodeViewStore store;
    NodeViewModel model;
    NodeViewGroupSortingModel groupSortingProxyModel;
    NodeViewItemDelegate itemDelegate;
};

TreeNodeView::Private::Private(
    TreeNodeView* owner,
    int columnCount)
    :
    owner(owner),
    model(columnCount, &store),
    itemDelegate(owner)
{
    groupSortingProxyModel.setSourceModel(&model);
}

void TreeNodeView::Private::handleExpanded(const QModelIndex& index)
{
    handleExpandStateChanged(index, true);
}

void TreeNodeView::Private::handleCollapsed(const QModelIndex& index)
{
    handleExpandStateChanged(index, false);
}

void TreeNodeView::Private::handleExpandStateChanged(const QModelIndex& index, bool expanded)
{
    if (const auto node = ViewNodeHelper::nodeFromIndex(index))
    {
        store.applyPatch(NodeViewStateReducer::setNodeExpandedPatch(
            store.state(), node->path(), expanded));
    }
    else
    {
        NX_ASSERT(false, "Wrong node!");
    }
}

void TreeNodeView::Private::updateExpandedState(const QModelIndex& index)
{
    const bool expandedNode = details::ViewNodeHelper(index).expanded();
    if (owner->isExpanded(index) != expandedNode)
        owner->setExpanded(index, expandedNode);
}

void TreeNodeView::Private::handleRowsInserted(const QModelIndex& parent, int from, int to)
{
    // We use model of owner instead of QModelIndex::child because parent index can be invalid.
    for (int row = from; row <= to; ++row)
        updateExpandedState(owner->model()->index(row, kAnyColumn, parent));
}

void TreeNodeView::Private::handlePatchApplied(const NodeViewStatePatch& patch)
{
    model.applyPatch(patch);

    for (const auto step: patch.steps)
    {
        if (step.operationData.operation == PatchStepOperation::removeDataOperation)
            continue;

        const auto& data = step.operationData.data.value<ViewNodeData>();
        if (!data.hasProperty(isExpandedProperty))
            continue;

        const auto index = getRootModelIndex(model.index(step.path, kAnyColumn), owner->model());
        updateExpandedState(index);
    }
}

//-------------------------------------------------------------------------------------------------

TreeNodeView::TreeNodeView(
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
        this, &TreeNodeView::handleDataChangeRequest);

    connect(&d->store, &details::NodeViewStore::patchApplied,
        d, &Private::handlePatchApplied);

    connect(this, &QTreeView::expanded, d, &Private::handleExpanded);
    connect(this, &QTreeView::collapsed, d, &Private::handleCollapsed);
    connect(&d->groupSortingProxyModel, &details::NodeViewModel::rowsInserted,
        d, &Private::handleRowsInserted);
}

TreeNodeView::~TreeNodeView()
{
}

void TreeNodeView::setProxyModel(QSortFilterProxyModel* proxy)
{
    d->groupSortingProxyModel.setSourceModel(proxy);
    proxy->setSourceModel(&d->model);
}

void TreeNodeView::applyPatch(const NodeViewStatePatch& patch)
{
    d->store.applyPatch(patch);
}

const details::NodeViewState& TreeNodeView::state() const
{
    return d->store.state();
}

void TreeNodeView::setupHeader()
{
}

const details::NodeViewStore& TreeNodeView::store() const
{
    return d->store;
}

details::NodeViewModel& TreeNodeView::sourceModel()
{
    return d->model;
}

void TreeNodeView::handleDataChangeRequest(
    const QModelIndex& index,
    const QVariant& value,
    int role)
{
    if (role != Qt::CheckStateRole)
        return;

    const auto node = ViewNodeHelper::nodeFromIndex(index);
    const auto checkedState = value.value<Qt::CheckState>();
    d->store.applyPatch(NodeViewStateReducer::setNodeChecked(
        d->store.state(), node->path(), {index.column()}, checkedState));
}

void TreeNodeView::setModel(QAbstractItemModel* model)
{
    NX_ASSERT(false, "NodeView does not allow to explicitly set model!");
}

} // namespace node_view
} // namespace nx::vms::client::desktop

