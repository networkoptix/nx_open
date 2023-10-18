// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "poe_settings_store.h"

#include <nx/vms/client/desktop/node_view/details/node/view_node_helper.h>
#include <nx/vms/client/desktop/node_view/details/node_view_store.h>
#include <nx/vms/client/desktop/node_view/node_view/node_view_state_reducer.h>

#include "poe_settings_reducer.h"

class QnResourcePool;

namespace nx::vms::client::desktop {

struct PoeSettingsStore::Private
{
    Private(PoeSettingsStore* store, QnResourcePool* resourcePool);
    void applyPatch(const PoeSettingsStatePatch& patch);

    PoeSettingsStore* const q;
    QnResourcePool* const pool;
    node_view::details::NodeViewStorePtr blockStore;
    node_view::details::NodeViewStorePtr totalsStore;
    PoeSettingsState state;
};

PoeSettingsStore::Private::Private(PoeSettingsStore* store, QnResourcePool* resourcePool):
    q(store),
    pool(resourcePool)
{
}

void PoeSettingsStore::Private::applyPatch(const PoeSettingsStatePatch& patch)
{
    if (blockStore)
        blockStore->applyPatch(patch.blockPatch);

    if (totalsStore)
        totalsStore->applyPatch(patch.totalsPatch);

    if (patch.hasChanges)
        state.hasChanges = patch.hasChanges.value();

    if (patch.showPreloader)
        state.showPreloader = patch.showPreloader.value();

    if (patch.showPoeOverBudgetWarning)
        state.showPoeOverBudgetWarning = patch.showPoeOverBudgetWarning.value();

    if (patch.blockUi)
        state.blockUi = patch.blockUi.value();

    if (patch.autoUpdates)
        state.autoUpdates = patch.autoUpdates.value();

    emit q->patchApplied(patch);
}

//--------------------------------------------------------------------------------------------------

PoeSettingsStore::PoeSettingsStore(QnResourcePool* resourcePool, QObject* parent):
    base_type(parent),
    d(new Private(this, resourcePool))
{
}

PoeSettingsStore::~PoeSettingsStore()
{
}

void PoeSettingsStore::setStores(
    const node_view::details::NodeViewStorePtr& blockTableStore,
    const node_view::details::NodeViewStorePtr& totalsTableStore)
{
    d->blockStore = blockTableStore;
    d->totalsStore = totalsTableStore;
}

const PoeSettingsState& PoeSettingsStore::state() const
{
    return d->state;
}

void PoeSettingsStore::updateBlocks(const nx::vms::api::NetworkBlockData& data)
{
    PoeSettingsStatePatch patch;
    patch.blockPatch = PoeSettingsReducer::blockDataChangesPatch(
        d->blockStore->state(), data, d->pool);

    patch.totalsPatch = PoeSettingsReducer::totalsDataChangesPatch(
        d->totalsStore->state(), data);

    patch.showPoeOverBudgetWarning = PoeSettingsReducer::poeOverBudgetChanges(d->state, data);

    d->applyPatch(patch);
}

void PoeSettingsStore::setHasChanges(bool value)
{
    if (d->state.hasChanges == value)
        return;

    PoeSettingsStatePatch patch;
    patch.hasChanges = value;

    d->applyPatch(patch);
}

void PoeSettingsStore::setBlockUi(bool value)
{
    PoeSettingsStatePatch patch;
    patch.blockUi = PoeSettingsReducer::blockUiChanges(d->state, value);

    d->applyPatch(patch);
}

void PoeSettingsStore::setPreloaderVisible(bool value)
{
    PoeSettingsStatePatch patch;
    patch.showPreloader = PoeSettingsReducer::showPreloaderChanges(d->state, value);

    d->applyPatch(patch);
}

void PoeSettingsStore::setAutoUpdates(bool value)
{
    PoeSettingsStatePatch patch;
    patch.autoUpdates = PoeSettingsReducer::autoUpdatesChanges(d->state, value);

    d->applyPatch(patch);
}

void PoeSettingsStore::applyUserChanges()
{
    PoeSettingsStatePatch patch;
    patch.blockPatch = node_view::NodeViewStateReducer::applyUserChangesPatch(
        d->blockStore->state());

    d->applyPatch(patch);
}

void PoeSettingsStore::resetUserChanges()
{
    PoeSettingsStatePatch patch;
    patch.hasChanges = false;

    patch.blockPatch = node_view::NodeViewStateReducer::resetUserChangesPatch(
        d->blockStore->state());

    d->applyPatch(patch);
}

} // namespace nx::vms::client::desktop
