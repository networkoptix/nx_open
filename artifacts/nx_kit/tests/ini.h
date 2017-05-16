#pragma once

#define NX_INI_FILE "nx_kit_c_wrapper_test.ini"
#define NX_INI_STRUCT \
{ \
    NX_INI_FLAG(1, testFlag, "Test bool param."); \
    NX_INI_INT(113, testInt, "Test int param"); \
    NX_INI_STRING("defaultString", testString, "Test string param"); \
}

#include "ini_config_c.h"
