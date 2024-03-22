// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "nx_network_rest_ini.h"

namespace nx::network::rest {

Ini& ini()
{
    static Ini ini;
    return ini;
}

} // namespace nx::network::rest
