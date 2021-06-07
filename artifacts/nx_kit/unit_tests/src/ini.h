// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#define NX_INI_FILE "test_c.ini"
#define NX_INI_STRUCT \
{ \
    NX_INI_FLAG(1, testFlag, "Test bool param."); \
    NX_INI_INT(113, testInt, "Test int param"); \
    NX_INI_STRING("defaultString", testString, "Test string param"); \
    NX_INI_FLOAT(0.42f, testFloat, "Test float param"); \
}

#include <ini_config_c.h>
