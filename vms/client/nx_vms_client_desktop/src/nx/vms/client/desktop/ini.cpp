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
    // Allow to connect to any cloud host in demo mode if cloud host is not passed explicitly.
    QString cloudHost(this->cloudHost);
    return cloudHost == "auto"
        || (cloudHost.isEmpty() && demoMode);
}

} // namespace nx::vms::client::desktop
