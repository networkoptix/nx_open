// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ini.h"

namespace nx::build_info {

Ini& ini()
{
    static Ini ini;
    return ini;
}

} // namespace nx::build_info
