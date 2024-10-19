// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ini.h"

namespace nx::mobile_client {

Ini& ini()
{
    static Ini ini;
    return ini;
}

} // namespace nx::mobile_client
