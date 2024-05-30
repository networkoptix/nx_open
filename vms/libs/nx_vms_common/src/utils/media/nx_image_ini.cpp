// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "nx_image_ini.h"

NxImageIniConfig& nxImageIni()
{
    static NxImageIniConfig ini;
    return ini;
}
