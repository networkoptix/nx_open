#include "node_view_widget.h"

#include <nx/client/desktop/resource_views/node_view/node_view_model.h>

#include <ui/style/helper.h>
#include <ui/common/indents.h>

namespace nx {
namespace client {
namespace desktop {

struct NodeViewWidget::Private
{
    NodeViewModel model;
};

//-------------------------------------------------------------------------------------------------

NodeViewWidget::NodeViewWidget(QWidget* parent):
    base_type(parent),
    d(new Private())
{
    setModel(&d->model);

    setProperty(style::Properties::kSideIndentation,
        QVariant::fromValue(QnIndents(0, 1)));
    setIndentation(style::Metrics::kDefaultIconSize);
}

NodeViewWidget::~NodeViewWidget()
{
}

void NodeViewWidget::loadState(const NodeViewState& state)
{
    d->model.loadState(state);
}

} // namespace desktop
} // namespace client
} // namespace nx

