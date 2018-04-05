// Copyright 2018 Network Optix, Inc. Licensed under GNU Lesser General Public License version 3.
#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#if defined(__cplusplus)
    extern "C" {
#endif

/**
 * Generate a unique .ini file name to avoid collisions in this test.
 */
const char* ini_detail_uniqueIniFileName();

#if defined(__cplusplus)
    } // extern "C"
#endif

#define NX_INI_FILE ini_detail_uniqueIniFileName()
#define NX_INI_STRUCT \
{ \
    NX_INI_FLAG(1, testFlag, "Test bool param."); \
    NX_INI_INT(113, testInt, "Test int param"); \
    NX_INI_STRING("defaultString", testString, "Test string param"); \
}

#include "ini_config_c.h"
