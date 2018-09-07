#pragma once

/**
 * Called from the C++ unit test for "ini_config_c.h". Performs some basic tests of
 * "ini_config_c.h". Needed to check that "ini_config_c.h" works when included into a C99 source
 * file.
 * @return Line number of the failed test, or 0 on success.
 */
int testIniConfigCUsage();
