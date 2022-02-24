// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_management_ini.h"

ResourceManagementIni& resourceManagementIni()
{
    static ResourceManagementIni ini;
    return ini;
}
