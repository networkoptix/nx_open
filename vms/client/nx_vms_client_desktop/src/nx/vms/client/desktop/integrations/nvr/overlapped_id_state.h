// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <nx/vms/client/desktop/common/flux/abstract_flux_state.h>
#include <nx/vms/client/desktop/common/flux/flux_types.h>

namespace nx::vms::client::desktop::integrations {

struct NX_VMS_CLIENT_DESKTOP_API OverlappedIdState: AbstractFluxState
{
    int currentId = -1;
    std::vector<int> idList;
};

} // namespace nx::vms::client::desktop::integrations
