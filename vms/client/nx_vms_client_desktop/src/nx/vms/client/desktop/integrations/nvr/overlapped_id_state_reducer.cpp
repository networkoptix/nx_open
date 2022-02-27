// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "overlapped_id_state_reducer.h"

namespace nx::vms::client::desktop::integrations {

using State = OverlappedIdState;

State OverlappedIdStateReducer::reset(State state)
{
    state.currentId = -1;
    state.idList.clear();

    return std::move(state);
}

State OverlappedIdStateReducer::setCurrentId(State state, int id)
{
    state.currentId = id;
    return std::move(state);
}

State OverlappedIdStateReducer::setIdList(State state, const std::vector<int>& idList)
{
    state.idList = idList;
    return std::move(state);
}

} // namespace nx::vms::client::desktop::integrations
