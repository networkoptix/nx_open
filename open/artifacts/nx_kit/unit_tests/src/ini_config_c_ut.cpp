#include <cstring>

#include <nx/kit/test.h>

#include "ini.h"

extern "C" {
    #include "ini_config_c_usage.h"
} // extern "C"

static void assertIniEquals(int line, const Ini& expected, const Ini& actual)
{
    #define ASSERT_INI_PARAM_EQ(PARAM) ASSERT_EQ_AT_LINE(line, expected.PARAM, actual.PARAM)
    #define ASSERT_INI_PARAM_STREQ(PARAM) ASSERT_STREQ_AT_LINE(line, expected.PARAM, actual.PARAM)

    ASSERT_INI_PARAM_EQ(testFlag);
    ASSERT_INI_PARAM_EQ(testInt);
    ASSERT_INI_PARAM_STREQ(testString);
    ASSERT_INI_PARAM_EQ(testFloat);
    ASSERT_INI_PARAM_EQ(testDouble);

    #undef ASSERT_INI_PARAM_EQ
    #undef ASSERT_INI_PARAM_STREQ
}

static void generateIniFile(const Ini& iniInstance)
{
    std::ostringstream content;

    #define GENERATE_INI_PARAM(PARAM) content << #PARAM << "=" << iniInstance.PARAM << std::endl

    GENERATE_INI_PARAM(testFlag);
    GENERATE_INI_PARAM(testInt);
    GENERATE_INI_PARAM(testString);
    GENERATE_INI_PARAM(testFloat);
    GENERATE_INI_PARAM(testDouble);

    #undef GENERATE_INI_PARAM

    nx::kit::test::createFile(nx_ini_iniFilePath(), content.str().c_str());
}

TEST(ini_config_c, test)
{
    const Ini defaultIni = ini; //< Source of default values for comparison.
    const Ini savedIni { false, 555, "new string", 36.6f, 42.42 };

    ASSERT_TRUE(nx_ini_isEnabled());
    ASSERT_STREQ(NX_INI_FILE, nx_ini_iniFile());

    nx_ini_setIniFilesDir(nx::kit::test::tempDir());
    ASSERT_STREQ(nx::kit::test::tempDir(), nx_ini_iniFilesDir());

    ASSERT_EQ(std::string(nx_ini_iniFilesDir()) + nx_ini_iniFile(), nx_ini_iniFilePath());

    nx_ini_reload(); //< Output expected: first time.
    nx_ini_reload(); //< No output expected: no changes.

    assertIniEquals(__LINE__, defaultIni, ini);

    generateIniFile(savedIni);

    nx_ini_reload(); //< Output expected: new values arrive.

    assertIniEquals(__LINE__, savedIni, ini);

    nx_ini_setOutput(NX_INI_OUTPUT_NONE);
    nx_ini_reload(); //< No output expected, though values change: output is disabled.

    ASSERT_EQ(0, testIniConfigCUsage());
}
