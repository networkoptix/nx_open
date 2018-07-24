#include "node_view.h"

#include <QtWidgets/QHeaderView>
#include <QtCore/QSortFilterProxyModel>

#include <ui/style/helper.h>
#include <ui/common/indents.h>

#include <nx/client/desktop/resource_views/node_view/node_view_state.h>
#include <nx/client/desktop/resource_views/node_view/node_view_constants.h>
#include <nx/client/desktop/resource_views/node_view/node/view_node_path.h>
#include <nx/client/desktop/resource_views/node_view/node/view_node_helpers.h>
#include <nx/client/desktop/resource_views/node_view/details/node_view_store.h>
#include <nx/client/desktop/resource_views/node_view/details/node_view_item_delegate.h>
#include <nx/client/desktop/resource_views/node_view/details/node_view_state_reducer.h>
#include <nx/client/desktop/resource_views/node_view/details/node_view_group_sorting_model.h>
#include <nx/client/desktop/resource_views/node_view/details/node_view_model.h>

#include <nx/client/desktop/common/utils/item_view_utils.h>

namespace {

QModelIndex getRootIndex(
    const QModelIndex& leafIndex,
    const QAbstractItemModel* rootModel)
{
    const auto proxyModel = qobject_cast<const QAbstractProxyModel*>(rootModel);
    if (!proxyModel)
        return rootModel->index(leafIndex.row(), leafIndex.column(), leafIndex.parent());

    const auto sourceIndex = getRootIndex(leafIndex, proxyModel->sourceModel());
    return proxyModel->mapFromSource(sourceIndex);
}

}

namespace nx {
namespace client {
namespace desktop {

struct NodeView::Private: public QObject
{
    Private(NodeView* owner);

    void updateColumns();
    void updateModelConnectinos();

    void handleExpanded(const QModelIndex& index);
    void handleCollapsed(const QModelIndex& index);
    void handlePatchApplied(const NodeViewStatePatch& patch);
    void handleRowsInserted(const QModelIndex& parent, int from, int to);

    NodeView * const owner;

    details::NodeViewStore store;
    details::NodeViewModel model;
    details::NodeViewGroupSortingModel groupSortingProxyModel;
    details::NodeViewItemDelegate itemDelegate;
    ItemViewUtils::CheckableCheckFunction checkableCheck;
};

NodeView::Private::Private(NodeView* owner):
    owner(owner),
    model(&store),
    itemDelegate(owner),
    checkableCheck([owner](const QModelIndex& index) { return helpers::isCheckable(index); })
{
}

void NodeView::Private::updateColumns()
{
    const auto header = owner->header();

    header->setStretchLastSection(false);

    header->setSectionResizeMode(node_view::nameColumn, QHeaderView::Stretch);
    header->setSectionResizeMode(node_view::checkMarkColumn, QHeaderView::ResizeToContents);

    if (!store.state().checkable())
        header->setSectionHidden(node_view::checkMarkColumn, true);
}

void NodeView::Private::handleExpanded(const QModelIndex& index)
{
    if (const auto node = helpers::nodeFromIndex(index))
        store.setNodeExpanded(node->path(), true);
    else
        NX_EXPECT(false, "Wrong node!");
}

void NodeView::Private::handleCollapsed(const QModelIndex& index)
{
    if (const auto node = helpers::nodeFromIndex(index))
        store.setNodeExpanded(node->path(), false);
    else
        NX_EXPECT(false, "Wrong node!");
}

void NodeView::Private::handlePatchApplied(const NodeViewStatePatch& patch)
{
    model.applyPatch(patch);
    for (const auto data: patch.changedData)
    {
        if (!helpers::hasExpandedData(data.data))
            continue; // No "expanded" data.

        const auto index = model.index(data.path, node_view::nameColumn);
        const auto rootIndex = getRootIndex(index, owner->model());
        const bool expanded = helpers::expanded(data.data);
        if (owner->isExpanded(rootIndex) != expanded)
            owner->setExpanded(rootIndex, expanded);
    }
}

void NodeView::Private::handleRowsInserted(const QModelIndex& parent, int from, int to)
{
    for (int row = from; row <= to; ++row)
    {
        const auto index = owner->model()->index(row, node_view::nameColumn, parent);
        const auto expandedData = index.data(node_view::expandedRole);
        if (helpers::hasExpandedData(index))
            owner->setExpanded(index, helpers::expanded(index));
    }
}

//-------------------------------------------------------------------------------------------------

NodeView::NodeView(QWidget* parent):
    base_type(parent),
    d(new Private(this))
{
    d->groupSortingProxyModel.setSourceModel(&d->model);
    setModel(&d->groupSortingProxyModel);
    setSortingEnabled(true);
    setProperty(style::Properties::kSideIndentation, QVariant::fromValue(QnIndents(0, 1)));
    setIndentation(style::Metrics::kDefaultIconSize);
    setItemDelegate(&d->itemDelegate);
    ItemViewUtils::setupDefaultAutoToggle(this, node_view::checkMarkColumn, d->checkableCheck);

    connect(&d->model, &details::NodeViewModel::checkedChanged,
        &d->store, &details::NodeViewStore::setNodeChecked);
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

const NodeViewState& NodeView::state() const
{
    return d->store.state();
}

void NodeView::applyPatch(const NodeViewStatePatch& patch)
{
    d->store.applyPatch(patch);
    d->updateColumns();
}

void NodeView::setProxyModel(QSortFilterProxyModel* proxy)
{
    d->groupSortingProxyModel.setSourceModel(proxy);
    proxy->setSourceModel(&d->model);
}

const details::NodeViewStore* NodeView::store() const
{
    return &d->store;
}

details::NodeViewModel* NodeView::sourceModel() const
{
    return &d->model;
}


} // namespace desktop
} // namespace client
} // namespace nx

