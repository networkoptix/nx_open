#include "node_view_store.h"

#include <QtCore/QScopedValueRollback>

#include <nx/client/desktop/resource_views/node_view/details/node_view_state_reducer.h>

namespace nx {
namespace client {
namespace desktop {
namespace details {

struct NodeViewStore::Private
{
    Private(NodeViewStore* owner);
    void applyPatch(const NodeViewStatePatch& patch);

    NodeViewStore* const q;
    NodeViewState state;
    bool actionInProgress = false;
};

NodeViewStore::Private::Private(NodeViewStore* owner):
    q(owner)
{
}

void NodeViewStore::Private::applyPatch(const NodeViewStatePatch& patch)
{
    if (actionInProgress)
        return;

    const QScopedValueRollback<bool> guard(actionInProgress, true);
    state = applyNodeViewPatch(std::move(state), patch);
    emit q->patchApplied(patch);
}

//-------------------------------------------------------------------------------------------------

NodeViewStore::NodeViewStore(QObject* parent):
    base_type(parent),
    d(new Private(this))
{
}

NodeViewStore::~NodeViewStore()
{
}

const NodeViewState& NodeViewStore::state() const
{
    return d->state;
}

void NodeViewStore::setNodeChecked(
    const ViewNodePath& path,
    int column,
    Qt::CheckState checkedState)
{
    d->applyPatch(details::NodeViewStateReducer::setNodeChecked(path, column, checkedState));
}

void NodeViewStore::setNodeExpanded(
    const ViewNodePath& path,
    bool expanded)
{
    d->applyPatch(details::NodeViewStateReducer::setNodeExpanded(path, expanded));
}

void NodeViewStore::applyPatch(const NodeViewStatePatch& patch)
{
    d->applyPatch(patch);
}

} // namespace details
} // namespace desktop
} // namespace client
} // namespace nx

