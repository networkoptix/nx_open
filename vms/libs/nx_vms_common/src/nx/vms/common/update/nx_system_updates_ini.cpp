// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "nx_system_updates_ini.h"

namespace nx::vms::common::update {

Ini::Ini(): IniConfig("nx_system_updates.ini") { reload(); }

Ini& ini()
{
    static Ini ini;
    return ini;
}

} // namespace nx::vms::common::update
