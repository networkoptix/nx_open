#include "node_view_widget.h"

#include <QtWidgets/QHeaderView>

#include <ui/style/helper.h>
#include <ui/common/indents.h>

#include <nx/client/desktop/resource_views/node_view/node_view_model.h>
#include <nx/client/desktop/resource_views/node_view/node_view_state.h>
#include <nx/client/desktop/resource_views/node_view/node_view_store.h>

#include <nx/client/desktop/resource_views/node_view/node_view_state_reducer.h>
#include <nx/client/desktop/resource_views/node_view/node_view_constants.h>

#include <nx/client/desktop/common/utils/item_view_utils.h>

namespace nx {
namespace client {
namespace desktop {

struct NodeViewWidget::Private: public QObject
{
    Private(NodeViewWidget& owner);

    void updateColumns();
    void handleClicked(const QModelIndex& index);
    void handleCheckedChanged(const ViewNode::Path& path, Qt::CheckState checkedState);
    void handlePatchApplied();
    void handleStateChanged();

    NodeViewWidget& owner;
    NodeViewModel model;
    NodeViewStore store;
};

NodeViewWidget::Private::Private(NodeViewWidget& owner):
    owner(owner)
{
}

void NodeViewWidget::Private::updateColumns()
{
    const auto header = owner.header();

    header->setStretchLastSection(false);

    header->setSectionResizeMode(node_view::nameColumn, QHeaderView::Stretch);
    header->setSectionResizeMode(node_view::checkMarkColumn, QHeaderView::ResizeToContents);

    if (!model.state().checkable())
        header->setSectionHidden(node_view::checkMarkColumn, true);
}

void NodeViewWidget::Private::handleCheckedChanged(
    const ViewNode::Path& path,
    Qt::CheckState checkedState)
{
    store.setNodeChecked(path, checkedState);
}

void NodeViewWidget::Private::handlePatchApplied()
{
    model.applyPatch(store.lastPatch());
}

void NodeViewWidget::Private::handleStateChanged()
{
    model.setState(store.state());
}

//-------------------------------------------------------------------------------------------------

NodeViewWidget::NodeViewWidget(QWidget* parent):
    base_type(parent),
    d(new Private(*this))
{
    setModel(&d->model);
    setProperty(style::Properties::kSideIndentation, QVariant::fromValue(QnIndents(0, 1)));
    setIndentation(style::Metrics::kDefaultIconSize);

    ItemViewUtils::setupDefaultAutoToggle(this, node_view::checkMarkColumn);

    connect(&d->store, &NodeViewStore::stateChanged, d, &Private::handleStateChanged);
    connect(&d->store, &NodeViewStore::patchApplied, d, &Private::handlePatchApplied);
    connect(&d->model, &NodeViewModel::checkedChanged, d, &Private::handleCheckedChanged);
}

NodeViewWidget::~NodeViewWidget()
{
}

void NodeViewWidget::setState(const NodeViewState& state)
{
    d->store.setState(state);
    d->updateColumns();
}

const NodeViewState& NodeViewWidget::state() const
{
    return d->store.state();
}

void NodeViewWidget::applyPatch(const NodeViewStatePatch& patch)
{
    d->store.applyPatch(patch);
}

} // namespace desktop
} // namespace client
} // namespace nx

