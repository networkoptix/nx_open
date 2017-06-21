// Copyright 2017 Network Optix, Inc. Licensed under GNU Lesser General Public License version 3.

#include <iostream>
#include <fstream>
#include <cstring>
#include <ctime>

#include <nx/kit/test.h>
#include <nx/kit/ini_config.h>

using namespace nx::kit;

/**
 * Generate a unique .ini file name to avoid collisions in this test.
 */
static std::string uniqueIniFileName()
{
    static bool randomized = false;
    if (!randomized)
    {
        srand((unsigned int) time(0));
        randomized = true;
    }
    constexpr int kMaxIniFileNameLen = 100;
    char iniFileName[kMaxIniFileNameLen];
    snprintf(iniFileName, kMaxIniFileNameLen, "%d_test.ini", rand());
    return std::string(iniFileName);
}

struct TestIniConfig: IniConfig
{
    using IniConfig::IniConfig;

    NX_INI_FLAG(0, enableOutput, "Enable Output.");
    NX_INI_FLAG(1, enableTime, "Enable Time.");
    NX_INI_FLAG(1, enableFps, "Enable Fps.");
    NX_INI_STRING("string with middle \" quote", str1, "String with middle quote.");
    NX_INI_STRING("\" string with leading quote", str2, "String with leading quote.");
    NX_INI_STRING("string with trailing quote \"", str3, "String with trailing quote.");
    NX_INI_STRING("\"enquoted string\"", str4, "Enquoted string.");
    NX_INI_STRING("simple string", str5, "Simple string.");
    NX_INI_INT(113, number, "Test number.");
};

static const TestIniConfig defaultIni("will_not_load_from_file.ini");

static const std::string iniFileName = uniqueIniFileName();

// Using a static instance just to test this possibility.
static TestIniConfig& ini()
{
    static TestIniConfig ini(iniFileName.c_str());
    return ini;
}

// Intended to be saved to file, but not to be loaded from file.
struct SavedIniConfig: IniConfig
{
    using IniConfig::IniConfig;

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
};

template<class ExpectedIni, class ActualIni>
static void assertIniEquals(const ExpectedIni& expected, const ActualIni& actual, int line)
{
    bool failed = false;

    #define NX_TEST_INI_EQ(PARAM, COND) do \
    { \
        if (!(COND)) \
        { \
            failed = true; \
            std::cerr << "Test FAILED at line " << line << ": " << #PARAM << " expected {" \
                << expected.PARAM << "}, actual {" << actual.PARAM << "}" << std::endl; \
        } \
    } while (0)

    #define NX_TEST_INI_EQ_VAL(PARAM) \
        NX_TEST_INI_EQ(PARAM, expected.PARAM == actual.PARAM)

    #define NX_TEST_INI_EQ_STR(PARAM) \
        NX_TEST_INI_EQ(PARAM, strcmp(expected.PARAM, actual.PARAM) == 0)

    NX_TEST_INI_EQ_VAL(enableOutput);
    NX_TEST_INI_EQ_VAL(enableTime);
    NX_TEST_INI_EQ_VAL(enableFps);
    NX_TEST_INI_EQ_VAL(number);
    NX_TEST_INI_EQ_STR(str1);
    NX_TEST_INI_EQ_STR(str2);
    NX_TEST_INI_EQ_STR(str3);
    NX_TEST_INI_EQ_STR(str4);
    NX_TEST_INI_EQ_STR(str5);

    #undef NX_TEST_INI_EQ_STR
    #undef NX_TEST_INI_EQ_VAL
    #undef NX_TEST_INI_EQ

    ASSERT_TRUE(!failed);
}

/**
 * Create a temporary .ini file with the specified contents; delete the file in the destructor.
 */
struct IniFile
{
    const std::string iniFilePath;

    template<class Ini>
    IniFile(const Ini& ini):
        iniFilePath(ini.iniFilePath())
    {
        std::ofstream file(iniFilePath); //< Create .ini file.
        ASSERT_TRUE(file.good());

        const auto outputStr = //< Enquote the string if and only if it contains any quotes.
            [&file](const char* value, const char* paramName)
            {
                file << paramName << "=";
                if (strchr(value, '"') != nullptr)
                    file << '"' << value << '"';
                else
                    file << value;
                file << std::endl;
            };

        #define NX_TEST_INI_OUTPUT_VAL(PARAM) file << #PARAM << "=" << ini.PARAM << std::endl
        #define NX_TEST_INI_OUTPUT_STR(PARAM) outputStr(ini.PARAM, #PARAM)

        NX_TEST_INI_OUTPUT_VAL(enableOutput);
        NX_TEST_INI_OUTPUT_VAL(enableTime);
        NX_TEST_INI_OUTPUT_VAL(enableFps);
        NX_TEST_INI_OUTPUT_VAL(number);
        NX_TEST_INI_OUTPUT_STR(str1);
        NX_TEST_INI_OUTPUT_STR(str2);
        NX_TEST_INI_OUTPUT_STR(str3);
        NX_TEST_INI_OUTPUT_STR(str4);
        NX_TEST_INI_OUTPUT_STR(str5);

        #undef NX_TEST_INI_OUTPUT_STR
        #undef NX_TEST_INI_OUTPUT_VAL
    }

    ~IniFile()
    {
        remove(iniFilePath.c_str()); //< Delete .ini file.
    }
};

enum class Output { silent, verbose };

template<class ExpectedIni>
static void testReload(
    Output reloadOutput, const ExpectedIni& expectedIni, const char* caseTitle, int line)
{
    std::cerr << std::endl;
    if (reloadOutput == Output::silent)
        std::cerr << "------- BEGIN Silent: " << caseTitle << "." << std::endl;
    else
        std::cerr << "+++++++ BEGIN Verbose: " << caseTitle << ":" << std::endl;

    std::ostringstream output;
    TestIniConfig::setOutput(&output);

    ini().reload();

    TestIniConfig::setOutput(&std::cerr); //< Restore global setting.
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
        if (ini().isEnabled())
            ASSERT_FALSE(outputStr.empty());
        else
            ASSERT_TRUE(outputStr.empty()); //< If disabled, there should be no output.
    }

    if (ini().isEnabled())
        assertIniEquals(expectedIni, ini(), line);
    else
        assertIniEquals(defaultIni, ini(), line); //< If disabled, values never change.
}

TEST(iniConfig, test)
{
    // Check path properties of IniConfig.
    ASSERT_STREQ(iniFileName.c_str(), ini().iniFile());
    ASSERT_EQ(std::string(ini().iniFileDir()) + iniFileName, ini().iniFilePath());

    std::cerr << "ini().isEnabled() -> " << (ini().isEnabled() ? "true" : "false") << std::endl;

    std::remove(ini().iniFilePath()); //< Clean up from failed runs (if any).

    testReload(Output::verbose, defaultIni, "first reload(), .ini file not found", __LINE__);
    testReload(Output::silent, defaultIni, ".ini file still not found", __LINE__);

    // Create/delete ini file with different values.
    {
        SavedIniConfig savedIni(ini().iniFile()); //< Create on the stack to test this possibility.
        IniFile iniFile(savedIni);

        testReload(Output::verbose, savedIni, "values changed", __LINE__);
        testReload(Output::silent, savedIni, "values not changed", __LINE__);
    }
}
