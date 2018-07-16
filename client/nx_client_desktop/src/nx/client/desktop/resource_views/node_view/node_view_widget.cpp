#include "node_view_widget.h"

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
#include <nx/client/desktop/resource_views/node_view/detail/node_view_helpers.h>


#include <nx/client/desktop/common/utils/item_view_utils.h>

namespace nx {
namespace client {
namespace desktop {

struct NodeViewWidget::Private: public QObject
{
    Private(NodeViewWidget* owner);

    void updateColumns();

    NodeViewWidget * const owner;
    NodeViewStore store;
    NodeViewModel model;
    QSortFilterProxyModel* proxy = nullptr;
    ItemViewUtils::CheckableCheckFunction checkableCheck;
};

NodeViewWidget::Private::Private(NodeViewWidget* owner):
    owner(owner),
    model(&store),
    checkableCheck(
        [owner](const QModelIndex& index) -> bool
        {
            const auto mappedIndex = detail::getSourceIndex(index, owner->model());
            const auto node = NodeViewModel::nodeFromIndex(mappedIndex);
            return node ? node->checkable() : false;
        })
{
}

void NodeViewWidget::Private::updateColumns()
{
    const auto header = owner->header();

    header->setStretchLastSection(false);

    header->setSectionResizeMode(node_view::nameColumn, QHeaderView::Stretch);
    header->setSectionResizeMode(node_view::checkMarkColumn, QHeaderView::ResizeToContents);

    if (!store.state().checkable())
        header->setSectionHidden(node_view::checkMarkColumn, true);
}

//-------------------------------------------------------------------------------------------------

NodeViewWidget::NodeViewWidget(QWidget* parent):
    base_type(parent),
    d(new Private(this))
{
    setModel(&d->model);
    setSortingEnabled(true);
    setProperty(style::Properties::kSideIndentation, QVariant::fromValue(QnIndents(0, 1)));
    setIndentation(style::Metrics::kDefaultIconSize);

    ItemViewUtils::setupDefaultAutoToggle(this, node_view::checkMarkColumn, d->checkableCheck);

    connect(&d->store, &NodeViewStore::patchApplied, &d->model, &NodeViewModel::applyPatch);
    connect(&d->model, &NodeViewModel::checkedChanged, &d->store, &NodeViewStore::setNodeChecked);
}

NodeViewWidget::~NodeViewWidget()
{
}

const NodeViewState& NodeViewWidget::state() const
{
    return d->store.state();
}

void NodeViewWidget::applyPatch(const NodeViewStatePatch& patch)
{
    d->store.applyPatch(patch);
    d->updateColumns();
}

void NodeViewWidget::setProxyModel(QSortFilterProxyModel* proxy)
{
    proxy->setSourceModel(&d->model);
    setModel(proxy);
}

} // namespace desktop
} // namespace client
} // namespace nx

