// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "config.h"

namespace nx::vms::client::desktop::integrations {

Config& config()
{
    static Config Config;
    return Config;
}

} // namespace nx::vms::client::desktop::integrations
