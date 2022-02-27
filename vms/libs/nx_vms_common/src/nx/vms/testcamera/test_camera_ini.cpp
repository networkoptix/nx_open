// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "test_camera_ini.h"

namespace nx::vms::testcamera {

Ini& ini()
{
    static Ini ini;
    return ini;
}

} // namespace nx::vms::testcamera
