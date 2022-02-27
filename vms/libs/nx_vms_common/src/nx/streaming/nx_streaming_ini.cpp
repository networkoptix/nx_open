// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "nx_streaming_ini.h"

NxStreamingIniConfig& nxStreamingIni()
{
    static NxStreamingIniConfig ini;
    return ini;
}
