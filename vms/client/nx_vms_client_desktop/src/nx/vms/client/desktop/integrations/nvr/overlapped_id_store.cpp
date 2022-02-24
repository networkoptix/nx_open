// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "overlapped_id_store.h"

#include <nx/vms/client/desktop/common/flux/private_flux_store.h>

#include "overlapped_id_state.h"
#include "overlapped_id_state_reducer.h"

namespace nx::vms::client::desktop::integrations {

using State = OverlappedIdState;
using Reducer = OverlappedIdStateReducer;

struct OverlappedIdStore::Private:
    PrivateFluxStore<OverlappedIdStore, State>
{
    using PrivateFluxStore::PrivateFluxStore;
};

OverlappedIdStore::OverlappedIdStore(QObject* parent):
    base_type(parent),
    d(new Private(this))
{
}

OverlappedIdStore::~OverlappedIdStore()
{
}

void OverlappedIdStore::reset()
{
    d->executeAction([&](State state) { return Reducer::reset(std::move(state)); });
}

void OverlappedIdStore::setCurrentId(int id)
{
    d->executeAction([&](State state) { return Reducer::setCurrentId(std::move(state), id); });
}

void OverlappedIdStore::setIdList(const std::vector<int>& idList)
{
    d->executeAction([&](State state) { return Reducer::setIdList(std::move(state), idList); });
}

const State& OverlappedIdStore::state() const
{
    return d->state;
}

} // namespace nx::vms::client::desktop::integrations
