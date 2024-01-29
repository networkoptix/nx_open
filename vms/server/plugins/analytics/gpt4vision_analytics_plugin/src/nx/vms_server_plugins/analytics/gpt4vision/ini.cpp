// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ini.h"

namespace nx::vms_server_plugins::analytics::gpt4vision {

Ini& ini()
{
    static Ini ini;
    return ini;
}

} // namespace nx::vms_server_plugins::analytics::gpt4vision
