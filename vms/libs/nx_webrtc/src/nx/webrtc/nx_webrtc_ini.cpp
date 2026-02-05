// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "nx_webrtc_ini.h"

#include <nx/utils/std_string_utils.h>

Ini& ini()
{
    static Ini ini;
    return ini;
}
