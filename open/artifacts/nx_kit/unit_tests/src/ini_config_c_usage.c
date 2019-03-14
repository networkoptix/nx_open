#include "ini_config_c_usage.h"

#include <stdio.h>
#include <string.h>

#include "ini.h"

#define ASSERT_TRUE(CONDITION) do \
{ \
    if (!(CONDITION)) \
    { \
        fprintf(stderr, "\nFAILURE at line %d, file %s:\n", __LINE__, __FILE__); \
        fprintf(stderr, "    Expected true, but is false: %s\n", #CONDITION); \
        return __LINE__; \
    } \
} while (0)

int testIniConfigCUsage()
{
    ASSERT_TRUE(nx_ini_isEnabled());

    ASSERT_TRUE(nx_ini_iniFile() != NULL);
    ASSERT_TRUE(strlen(nx_ini_iniFile()) > 0);

    ASSERT_TRUE(nx_ini_iniFilesDir() != NULL);
    ASSERT_TRUE(strlen(nx_ini_iniFilesDir()) > 0);

    ASSERT_TRUE(nx_ini_iniFilePath() != NULL);
    ASSERT_TRUE(strlen(nx_ini_iniFilePath()) > 0);

    // Test: iniFilePath() == iniFilesDir() + iniFile().
    char iniFilePath[1000] = "";
    snprintf(iniFilePath, sizeof(iniFilePath), "%s%s", nx_ini_iniFilesDir(), nx_ini_iniFile());
    ASSERT_TRUE(strcmp(iniFilePath, nx_ini_iniFilePath()) == 0);

    return 0;
}
