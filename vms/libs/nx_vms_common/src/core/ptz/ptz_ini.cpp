// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ptz_ini.h"

PtzIni& ptzIni()
{
    static PtzIni ini;
    return ini;
}
