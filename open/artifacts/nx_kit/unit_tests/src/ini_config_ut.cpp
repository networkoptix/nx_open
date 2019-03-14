/**@file
 * NOTE: Intended to be compiled twice - both with and without -DNX_INI_CONFIG_DISABLED. This is
 * needed to test ini_config in both Enabled and Disabled modes. To avoid symbol clashing, one or
 * both of the two compilations should produce a dynamic library.
 */

#include <iostream>
#include <fstream>
#include <cstring>
#include <ctime>

#include <nx/kit/test.h>
#include <nx/kit/ini_config.h>

using namespace nx::kit;

static constexpr char kIniFile[] = "test.ini";

struct TestIni: IniConfig
{
    TestIni(): IniConfig(kIniFile) {}

    NX_INI_FLAG(0, enableOutput, "Enable Output.");
    NX_INI_FLAG(1, enableTime, "Enable Time.");
    NX_INI_FLAG(1, enableFps, "Enable Fps.");
    NX_INI_STRING("string with middle \" quote", str1, "String with middle quote.");
    NX_INI_STRING("\" string with leading quote", str2, "String with leading quote.");
    NX_INI_STRING("string with trailing quote \"", str3, "String with trailing quote.");
    NX_INI_STRING("\"enquoted string\"", str4, "Enquoted string.");
    NX_INI_STRING("simple string", str5, "Simple string.");
    NX_INI_INT(113, number, "Test number.");
    NX_INI_FLOAT(310.55f, floatNumber ,"Test float number.");
    NX_INI_DOUBLE(-0.45, doubleNumber, "Test double number.");
};

static const TestIni defaultIni; //< Source of default values for comparison.

// Using a static instance just to test this possibility.
static TestIni& ini()
{
    static TestIni ini;
    return ini;
}

// Intended to be saved to the same file as TestIni, but not to be loaded from file.
struct SavedIni: IniConfig
{
    SavedIni(): IniConfig(ini().iniFile()) {}

    NX_INI_STRING(nullptr, nullStr, "Null string."); //< Test the recovery from null values.

    NX_INI_FLAG(1, enableOutput, "Enable Output.");
    NX_INI_FLAG(0, enableTime, "Enable Time.");
    NX_INI_FLAG(0, enableFps, "Enable Fps.");
    NX_INI_STRING("Another string with middle \" quote", str1, "String with middle quote.");
    NX_INI_STRING("\" Another string with leading quote", str2, "String with leading quote.");
    NX_INI_STRING("Another string with trailing quote \"", str3, "String with trailing quote.");
    NX_INI_STRING("\"Another enquoted string\"", str4, "Enquoted string.");
    NX_INI_STRING("Another simple string", str5, "Simple string.");
    NX_INI_INT(777, number, "Test number.");
    NX_INI_FLOAT(0.432f, floatNumber ,"Test float number.");
    NX_INI_DOUBLE(34.45, doubleNumber, "Test double number.");
};

template<class ExpectedIni, class ActualIni>
static void assertIniEquals(int line, const ExpectedIni& expected, const ActualIni& actual)
{
    #define ASSERT_INI_PARAM_EQ(PARAM) ASSERT_EQ_AT_LINE(line, expected.PARAM, actual.PARAM)
    #define ASSERT_INI_PARAM_STREQ(PARAM) ASSERT_STREQ_AT_LINE(line, expected.PARAM, actual.PARAM)

    ASSERT_INI_PARAM_EQ(enableOutput);
    ASSERT_INI_PARAM_EQ(enableTime);
    ASSERT_INI_PARAM_EQ(enableFps);
    ASSERT_INI_PARAM_STREQ(str1);
    ASSERT_INI_PARAM_STREQ(str2);
    ASSERT_INI_PARAM_STREQ(str3);
    ASSERT_INI_PARAM_STREQ(str4);
    ASSERT_INI_PARAM_STREQ(str5);
    ASSERT_INI_PARAM_EQ(number);
    ASSERT_INI_PARAM_EQ(floatNumber);
    ASSERT_INI_PARAM_EQ(doubleNumber);

    #undef ASSERT_INI_PARAM_EQ
    #undef ASSERT_INI_PARAM_STREQ
}

/**
 * Create a .ini file with the contents from the supplied IniConfig object.
 */
void generateIniFile(const SavedIni& ini)
{
    std::ostringstream content;

    const auto outputStr = //< Enquote the string if and only if it contains any quotes.
        [&content](const char* value, const char* paramName)
        {
            content << paramName << "=";
            if (strchr(value, '"') != nullptr)
                content << '"' << value << '"';
            else
                content << value;
            content << std::endl;
        };

    #define GENERATE_INI_PARAM_VAL(PARAM) content << #PARAM << "=" << ini.PARAM << std::endl
    #define GENERATE_INI_PARAM_STR(PARAM) outputStr(ini.PARAM, #PARAM)

    GENERATE_INI_PARAM_VAL(enableOutput);
    GENERATE_INI_PARAM_VAL(enableTime);
    GENERATE_INI_PARAM_VAL(enableFps);
    GENERATE_INI_PARAM_STR(str1);
    GENERATE_INI_PARAM_STR(str2);
    GENERATE_INI_PARAM_STR(str3);
    GENERATE_INI_PARAM_STR(str4);
    GENERATE_INI_PARAM_STR(str5);
    GENERATE_INI_PARAM_VAL(number);
    GENERATE_INI_PARAM_VAL(floatNumber);
    GENERATE_INI_PARAM_VAL(doubleNumber);

    #undef GENERATE_INI_PARAM_STR
    #undef GENERATE_INI_PARAM_VAL

    nx::kit::test::createFile(ini.iniFilePath(), content.str().c_str());
}

enum class Output { silent, verbose };

template<class ExpectedIni>
static void testReload(
    int line, Output reloadOutput, const ExpectedIni& expectedIni, const char* caseTitle)
{
    std::cerr << std::endl;
    if (reloadOutput == Output::silent)
        std::cerr << "------- BEGIN Silent: " << caseTitle << "." << std::endl;
    else
        std::cerr << "+++++++ BEGIN Verbose: " << caseTitle << ":" << std::endl;

    std::ostringstream output;
    IniConfig::setOutput(&output);

    ini().reload();

    IniConfig::setOutput(&std::cerr); //< Restore global setting.
    const std::string outputStr = output.str();

    std::cerr << outputStr; //< Print what reload() has produced (if anything).

    if (reloadOutput == Output::silent)
    {
        std::cerr << "------- END" << std::endl;
        ASSERT_TRUE(outputStr.empty());
    }
    else
    {
        std::cerr << "+++++++ END" << std::endl;
        if (IniConfig::isEnabled())
            ASSERT_FALSE(outputStr.empty());
        else
            ASSERT_TRUE(outputStr.empty()); //< If disabled, there should be no output.
    }

    if (IniConfig::isEnabled())
        assertIniEquals(line, expectedIni, ini());
    else
        assertIniEquals(line, defaultIni, ini()); //< If disabled, values never change.
}

#include <nx/kit/debug.h>

TEST(iniConfig, test)
{
    std::cerr << "IniConfig::isEnabled() -> " << (IniConfig::isEnabled() ? "true" : "false")
        << std::endl;

    // Avoiding nx::kit::test::tempDir() for disabled ini_config to avoid dir name conflict.
    static const char* const tempDir = IniConfig::isEnabled() ? nx::kit::test::tempDir() : "stub";
    IniConfig::setIniFilesDir(tempDir);
    ASSERT_STREQ(tempDir, IniConfig::iniFilesDir());

    // Check path properties of IniConfig.
    ASSERT_STREQ(kIniFile, ini().iniFile());
    ASSERT_STREQ(std::string(IniConfig::iniFilesDir()) + kIniFile, ini().iniFilePath());

    testReload(__LINE__, Output::verbose, defaultIni, "first reload(), .ini file not found");
    testReload(__LINE__, Output::silent, defaultIni, ".ini file still not found");

    // Create/delete ini file with different values.
    SavedIni savedIni; //< Create on the stack to test this possibility.
    if (IniConfig::isEnabled())
        generateIniFile(savedIni);
    testReload(__LINE__, Output::verbose, savedIni, "values changed");
    testReload(__LINE__, Output::silent, savedIni, "values not changed");
}
