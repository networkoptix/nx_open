// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_services_ini.h"

namespace nx::cloud {

Ini& ini()
{
    static Ini ini;
    return ini;
}

} // namespace nx::cloud
