// Copyright 2018 Network Optix, Inc. Licensed under GNU Lesser General Public License version 3.
#include "ini.h"

extern "C" {

const char* ini_detail_uniqueIniFileName()
{
    static char iniFileName[100];
    static bool firstTime = true;
    if (firstTime)
    {
        firstTime = false;
        srand((unsigned int) time(nullptr));
        snprintf(iniFileName, sizeof(iniFileName), "%d_test.ini", rand());
    }
    return iniFileName;
}

} // extern "C"
