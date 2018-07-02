#include "node_view_widget.h"

#include <QtWidgets/QHeaderView>

#include <ui/style/helper.h>
#include <ui/common/indents.h>

#include <nx/client/desktop/resource_views/node_view/node_view_model.h>
#include <nx/client/desktop/resource_views/node_view/node_view_state.h>
#include <nx/client/desktop/resource_views/node_view/node_view_store.h>
#include <nx/client/desktop/resource_views/node_view/node_view_constants.h>

#include <nx/client/desktop/resource_views/node_view/node_view_state_reducer.h>

namespace nx {
namespace client {
namespace desktop {

struct NodeViewWidget::Private
{
    Private(NodeViewWidget& owner);

    void updateColumns();

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

    header->setSectionResizeMode(NodeViewColumn::Name, QHeaderView::Stretch);
    header->setSectionResizeMode(NodeViewColumn::CheckMark, QHeaderView::ResizeToContents);

    if (!model.state().checkable())
        header->setSectionHidden(NodeViewColumn::CheckMark, true);
}

//-------------------------------------------------------------------------------------------------

NodeViewWidget::NodeViewWidget(QWidget* parent):
    base_type(parent),
    d(new Private(*this))
{
    setModel(&d->model);
    setProperty(style::Properties::kSideIndentation,
        QVariant::fromValue(QnIndents(0, 1)));
    setIndentation(style::Metrics::kDefaultIconSize);

    connect(&d->store, &NodeViewStore::stateChanged, &d->model,
        [this]()
        {
            d->model.setState(d->store.state());
        });

    connect(&d->model, &NodeViewModel::checkedChanged, &d->store,
        [this](const ViewNode::Path& path, Qt::CheckState checkedState)
        {
            d->store.setNodeChecked(path, checkedState);
        });
}

NodeViewWidget::~NodeViewWidget()
{
}

void NodeViewWidget::loadState(const NodeViewState& state)
{
    d->store.setState(state);
    d->updateColumns();
}

} // namespace desktop
} // namespace client
} // namespace nx

