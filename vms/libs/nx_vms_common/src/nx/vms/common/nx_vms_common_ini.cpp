// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "nx_vms_common_ini.h"

namespace nx::vms::common {

Ini::Ini(): IniConfig("nx_vms_common.ini") { reload(); }

Ini& ini()
{
    static Ini ini;
    return ini;
}

} // namespace nx::vms::common
