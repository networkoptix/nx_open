// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/instrument.h>
#include <nx/utils/software_version.h>

namespace nx::vms::client::desktop {

struct SystemUpdateState
{
    QString file;
    int state = 0;
    // Update version we have initiated update for.
    nx::utils::SoftwareVersion version;

    bool operator==(const SystemUpdateState&) const = default;
};
NX_REFLECTION_INSTRUMENT(SystemUpdateState, (file)(state)(version))

} // namespace nx::vms::client::desktop
