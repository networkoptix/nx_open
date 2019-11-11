#include "poe_settings_store.h"

#include "poe_settings_reducer.h"

#include <common/common_module_aware.h>
#include <nx/vms/client/desktop/node_view/details/node_view_store.h>
#include <nx/vms/client/desktop/node_view/details/node/view_node_helpers.h>
#include <nx/vms/client/desktop/node_view/node_view/node_view_state_reducer.h>

class QnResourcePool;

namespace nx::vms::client::desktop {
namespace settings {

struct PoESettingsStore::Private
{
    Private(PoESettingsStore* store, QnCommonModuleAware* commonModuleAware);
    void applyPatch(const PoESettingsStatePatch& patch);

    PoESettingsStore* const q;
    QnResourcePool* const pool;
    node_view::details::NodeViewStorePtr blockTableStore;
    node_view::details::NodeViewStorePtr totalsTableStore;
    PoESettingsState state;
};

PoESettingsStore::Private::Private(PoESettingsStore* store, QnCommonModuleAware* commonModuleAware):
    q(store),
    pool(commonModuleAware->resourcePool())
{
}

void PoESettingsStore::Private::applyPatch(const PoESettingsStatePatch& patch)
{
    if (blockTableStore)
        blockTableStore->applyPatch(patch.blockPatch);

    if (patch.hasChanges)
    {
        state.hasChanges = patch.hasChanges.value();
    }

    // TODO: all other stuff

    emit q->patchApplied(patch);
}

//--------------------------------------------------------------------------------------------------

PoESettingsStore::PoESettingsStore(QnCommonModuleAware* commonModuleAware, QObject* parent):
    base_type(parent),
    d(new Private(this, commonModuleAware))
{
}

PoESettingsStore::~PoESettingsStore()
{
}

void PoESettingsStore::setStores(
    const node_view::details::NodeViewStorePtr& blockTableStore,
    const node_view::details::NodeViewStorePtr& totalsTableStore)
{
    d->blockTableStore = blockTableStore;
    d->totalsTableStore = totalsTableStore;
}

const PoESettingsState& PoESettingsStore::state() const
{
    return d->state;
}

void PoESettingsStore::updateBlocks(const core::PoEController::OptionalBlockData& blockData)
{
    const auto& blockState = d->blockTableStore->state();
    PoESettingsStatePatch patch;
    patch.blockPatch = PoESettingsReducer::blockDataChangesPatch(
        blockState, blockData, d->pool);

    d->applyPatch(patch);
}

void PoESettingsStore::setHasChanges(bool hasChanges)
{
    if (d->state.hasChanges == hasChanges)
        return;

    PoESettingsStatePatch patch;
    patch.hasChanges = hasChanges;

    d->applyPatch(patch);
}

void PoESettingsStore::applyUserChanges()
{
    PoESettingsStatePatch patch;
    patch.blockPatch = node_view::NodeViewStateReducer::applyUserChangesPatch(
        d->blockTableStore->state());

    d->applyPatch(patch);
}


} // namespace settings
} // namespace nx::vms::client::desktop

