#include "node_view_store.h"

#include "node_view_state.h"
#include "node_view_state_patch.h"

#include <QtCore/QScopedValueRollback>

namespace nx::vms::client::desktop {
namespace node_view {
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
    state = patch.applyTo(std::move(state));
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

void NodeViewStore::applyPatch(const NodeViewStatePatch& patch)
{
    d->applyPatch(patch);
}

bool NodeViewStore::applyingPatch() const
{
    return d->actionInProgress;
}

} // namespace details
} // namespace node_view
} // namespace nx::vms::client::desktop

