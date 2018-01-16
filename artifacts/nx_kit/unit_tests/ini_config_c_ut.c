// Copyright 2017 Network Optix, Inc. Licensed under GNU Lesser General Public License version 3.
#include "ini_config_c_ut.h"

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

TEST(ini_config_c_ut)
{
    const bool defaultFlag = ini.testFlag;
    const int defaultInt = ini.testInt;
    const char* const defaultString = ini.testString;

    ASSERT_TRUE(nx_ini_isEnabled());
    ASSERT_STREQ(NX_INI_FILE, nx_ini_iniFile());

    // Test: iniFilePath() == iniFileDir() + iniFile().
    char iniFilePath[1000] = "";
    strncat(iniFilePath, nx_ini_iniFileDir(), sizeof(iniFilePath) - 1);
    strncat(iniFilePath, nx_ini_iniFile(), sizeof(iniFilePath) - 1);
    ASSERT_STREQ(iniFilePath, nx_ini_iniFilePath());

    nx_ini_reload(); //< Output expected: first time.
    nx_ini_reload(); //< No output expected: no changes.

    ASSERT_EQ(defaultFlag, ini.testFlag);
    ASSERT_EQ(defaultInt, ini.testInt);
    ASSERT_STREQ(defaultString, ini.testString);

    // Create directory for ini files. Works for Windows as well.
    char mkdirCommand[1000] = "mkdir ";
    strncat(mkdirCommand, nx_ini_iniFileDir(), sizeof(mkdirCommand) - 1);
    system(mkdirCommand); //< Ignore possible errors.

    createFile(nx_ini_iniFilePath(),
        "testFlag=" STR(NEW_FLAG) "\n"
        "testInt=" STR(NEW_INT) "\n"
        "testString=" NEW_STRING "\n"
    );

    nx_ini_reload(); //< Output expected: new values arrive.

    ASSERT_EQ(NEW_FLAG, ini.testFlag);
    ASSERT_EQ(NEW_INT, ini.testInt);
    ASSERT_STREQ(NEW_STRING, ini.testString);

    remove(nx_ini_iniFilePath());

    nx_ini_setOutput(NX_INI_OUTPUT_NONE);
    nx_ini_reload(); //< No output expected, though values change: output is disabled.
}
