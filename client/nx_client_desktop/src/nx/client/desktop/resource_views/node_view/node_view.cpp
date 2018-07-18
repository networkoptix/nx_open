#include "node_view.h"

#include <QtWidgets/QHeaderView>
#include <QtCore/QSortFilterProxyModel>

#include <ui/style/helper.h>
#include <ui/common/indents.h>

#include <nx/client/desktop/resource_views/node_view/node_view_model.h>
#include <nx/client/desktop/resource_views/node_view/node_view_state.h>
#include <nx/client/desktop/resource_views/node_view/node_view_store.h>
#include <nx/client/desktop/resource_views/node_view/node_view_state_reducer.h>
#include <nx/client/desktop/resource_views/node_view/node_view_constants.h>
#include <nx/client/desktop/resource_views/node_view/nodes/view_node_path.h>
#include <nx/client/desktop/resource_views/node_view/sort/node_view_group_sorting_model.h>
#include <nx/client/desktop/resource_views/node_view/details/node_view_helpers.h>
#include <nx/client/desktop/resource_views/node_view/details/node_view_item_delegate.h>

#include <nx/client/desktop/common/utils/item_view_utils.h>

namespace nx {
namespace client {
namespace desktop {

struct NodeView::Private: public QObject
{
    Private(NodeView* owner);

    void updateColumns();

    NodeView * const owner;
    NodeViewStore store;

    NodeViewModel model;
    NodeViewGroupSortingModel groupSortingProxyModel;
    details::NodeViewItemDelegate itemDelegate;
    ItemViewUtils::CheckableCheckFunction checkableCheck;
};

NodeView::Private::Private(NodeView* owner):
    owner(owner),
    model(&store),
    itemDelegate(owner),
    checkableCheck(
        [owner](const QModelIndex& index) -> bool
        {
            const auto mappedIndex = details::getLeafIndex(index, owner->model());
            const auto node = NodeViewModel::nodeFromIndex(mappedIndex);
            return node ? node->checkable() : false;
        })
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

    connect(&d->store, &NodeViewStore::patchApplied, &d->model, &NodeViewModel::applyPatch);
    connect(&d->model, &NodeViewModel::checkedChanged, &d->store, &NodeViewStore::setNodeChecked);
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

const NodeViewStore* NodeView::store() const
{
    return &d->store;
}

NodeViewModel* NodeView::sourceModel() const
{
    return &d->model;
}


} // namespace desktop
} // namespace client
} // namespace nx

