// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

/**
 * Test ini_config in Disabled mode. The build system should define -DNX_INI_CONFIG_DISABLED when
 * compiling ini_config implementation file(s).
 */
DISABLED_INI_CONFIG_UT_API int disabled_ini_config_ut();
