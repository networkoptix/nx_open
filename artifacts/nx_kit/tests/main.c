/**@file
 * C99 test for ini_config_c.h.
 */

#include "test_c.h"

#include "ini.h"

static void createFile(const char* filename, const char* content)
{
    FILE* fp = fopen(filename, "w");
    if (!fp)
    {
        fprintf(stderr, "ERROR: Unable to create file %s\n", filename);
        exit(2);
    }

    if (fputs(content, fp) < 0)
    {
        fprintf(stderr, "ERROR: Unable to write to file %s\n", filename);
        exit(2);
    }

    fclose(fp);
}

#define NEW_FLAG false
#define NEW_INT 555
#define NEW_STRING "new string"

#define STR(VALUE) STR_DETAIL(VALUE)
#define STR_DETAIL(VALUE) #VALUE

TEST()
{
    const bool defaultFlag = ini.testFlag;
    const int defaultInt = ini.testInt;
    const char* const defaultString = ini.testString;

    ASSERT_TRUE(ini_isEnabled());
    ASSERT_STREQ(NX_INI_FILE, ini_iniFile());

    // Test: iniFilePath() == iniFileDir() + iniFile().
    char iniFilePath[1000] = "";
    strncat(iniFilePath, ini_iniFileDir(), sizeof(iniFilePath) - 1);
    strncat(iniFilePath, ini_iniFile(), sizeof(iniFilePath) - 1);
    ASSERT_STREQ(iniFilePath, ini_iniFilePath());

    ini_reload(); //< Output expected: first time.
    ini_reload(); //< No output expected: no changes.

    ASSERT_EQ(defaultFlag, ini.testFlag);
    ASSERT_EQ(defaultInt, ini.testInt);
    ASSERT_STREQ(defaultString, ini.testString);

    createFile(ini_iniFilePath(),
        "testFlag=" STR(NEW_FLAG) "\n"
        "testInt=" STR(NEW_INT) "\n"
        "testString=" NEW_STRING "\n"
    );

    ini_reload(); //< Output expected: new values arrive.

    ASSERT_EQ(NEW_FLAG, ini.testFlag);
    ASSERT_EQ(NEW_INT, ini.testInt);
    ASSERT_STREQ(NEW_STRING, ini.testString);

    remove(ini_iniFilePath());

    ini_setOutput(NX_INI_OUTPUT_NONE);
    ini_reload(); //< No output expected, though values change: output is disabled.
}
