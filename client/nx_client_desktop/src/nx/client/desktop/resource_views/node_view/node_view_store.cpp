#include "node_view_store.h"

#include <QtCore/QScopedValueRollback>

#include <nx/client/desktop/resource_views/node_view/node_view_state_reducer.h>

namespace nx {
namespace client {
namespace desktop {

struct NodeViewStore::Private
{
    void applyPatch(const NodeViewStatePatch& patch);

    NodeViewStore* const q;
    NodeViewState state;
    bool actionInProgress = false;
};

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
    d(new Private({this}))
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
    Qt::CheckState checkedState)
{
    d->applyPatch(NodeViewStateReducer::setNodeChecked(d->state, path, checkedState));
}

void NodeViewStore::applyPatch(const NodeViewStatePatch& patch)
{
    d->applyPatch(patch);
}

} // namespace desktop
} // namespace client
} // namespace nx

