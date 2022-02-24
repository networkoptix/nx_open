// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QVector>

#include <core/resource/resource_fwd.h>

#include "overlapped_id_state.h"

namespace nx::vms::client::desktop::integrations {

class NX_VMS_CLIENT_DESKTOP_API OverlappedIdStateReducer
{
public:
    using State = OverlappedIdState;

    static State reset(State state);
    static State setCurrentId(State state, int id);
    static State setIdList(State state, const std::vector<int>& idList);
};

} // namespace nx::vms::client::desktop::integrations
