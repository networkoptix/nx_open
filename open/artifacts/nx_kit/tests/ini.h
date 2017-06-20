// Copyright 2017 Network Optix, Inc. Licensed under GNU Lesser General Public License version 3.
#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/**
 * Generate a unique .ini file name to avoid collisions in this test.
 */
static const char* ini_detail_uniqueIniFileName()
{
    static char iniFileName[100];
    static bool firstTime = true;
    if (firstTime)
    {
        firstTime = false;
        srand((unsigned int) time(0));
        snprintf(iniFileName, sizeof(iniFileName), "%d_test.ini", rand());
    }
    return iniFileName;
}

#define NX_INI_FILE ini_detail_uniqueIniFileName()
#define NX_INI_STRUCT \
{ \
    NX_INI_FLAG(1, testFlag, "Test bool param."); \
    NX_INI_INT(113, testInt, "Test int param"); \
    NX_INI_STRING("defaultString", testString, "Test string param"); \
}

#include "ini_config_c.h"
