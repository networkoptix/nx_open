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
        state.hasChanges = patch.hasChanges.value();

    if (patch.showPoEOverBudgetWarning)
        state.showPoEOverBudgetWarning = patch.showPoEOverBudgetWarning.value();

    if (patch.blockUi)
        state.blockUi = patch.blockUi.value();
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

void PoESettingsStore::updateBlocks(const nx::vms::api::NetworkBlockData& data)
{
    PoESettingsStatePatch patch;
    patch.blockPatch = PoESettingsReducer::blockDataChangesPatch(
        d->blockTableStore->state(), data, d->pool);

    patch.totalsPatch = PoESettingsReducer::totalsDataChangesPatch(
        d->totalsTableStore->state(), data);

    patch.showPoEOverBudgetWarning = PoESettingsReducer::poeOverBudgetChanges(d->state, data);

    d->applyPatch(patch);
}

void PoESettingsStore::setHasChanges(bool value)
{
    if (d->state.hasChanges == value)
        return;

    PoESettingsStatePatch patch;
    patch.hasChanges = value;

    d->applyPatch(patch);
}

void PoESettingsStore::setBlockUi(bool value)
{
    PoESettingsStatePatch patch;
    patch.blockUi = PoESettingsReducer::blockUiChanges(d->state, value);

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

