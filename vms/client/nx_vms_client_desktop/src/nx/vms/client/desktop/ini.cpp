// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ini.h"

#include <nx/utils/log/assert.h>

namespace nx::vms::client::desktop {

Ini& ini()
{
    static Ini ini;
    return ini;
}

bool Ini::isAutoCloudHostDeductionMode() const
{
    return strcmp(cloudHost, "auto") == 0;
}

} // namespace nx::vms::client::desktop
