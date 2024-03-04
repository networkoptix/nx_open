// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

/**@file
 * NOTE: Intended to be compiled twice - both with and without -DNX_INI_CONFIG_DISABLED. This is
 * needed to test ini_config in both Enabled and Disabled modes. To avoid symbol clashing, one or
 * both of the two compilations should produce a dynamic library.
 */

#include <cstring>
#include <ctime>
#include <fstream>
#include <iostream>
#include <string>

#include <nx/kit/test.h>
#include <nx/kit/ini_config.h>

using namespace nx::kit;

static constexpr char kIniFile[] = "test.ini";

namespace nx {
namespace kit {

std::ostream& operator<<(std::ostream& s, IniConfig::ParamType value)
{
    switch (value)
    {
        case IniConfig::ParamType::boolean:
            return s << "boolean";

        case IniConfig::ParamType::integer:
            return s << "integer";

        case IniConfig::ParamType::float_:
            return s << "float";

        case IniConfig::ParamType::string:
            return s << "string";
    }

    return s << "Unknown type: " << (int) value;
}

} // namespace kit
} // namespace nx

struct TestIni: IniConfig
{
    TestIni(): IniConfig(kIniFile) {}

    NX_INI_FLAG(0, enableOutput, "Enable Output.");
    NX_INI_FLAG(1, enableTime, "Enable Time.");
    NX_INI_FLAG(1, enableFps, "Enable Fps.");
    NX_INI_STRING(" string with leading space", str0, "String with leading space.");
    NX_INI_STRING("string with middle \" quote", str1, "String with middle quote.");
    NX_INI_STRING("\" string with leading quote", str2, "String with leading quote.");
    NX_INI_STRING("string with trailing quote \"", str3, "String with trailing quote.");
    NX_INI_STRING("\"enquoted string\"", str4, "Enquoted string.");
    NX_INI_STRING("plain string", str5, "Plain string.");
    NX_INI_STRING("plain string with \\ backslash", str6, "Plain string with backslash.");
    NX_INI_INT(113, intNumber, "Test int number.");
    NX_INI_FLOAT(310.55F, floatNumber ,"Test float number.");
};

static const TestIni defaultIni; //< Source of default values for comparison.

/** Using a static instance just to test this possibility. */
static TestIni& ini()
{
    static TestIni ini;
    return ini;
}

/** Intended to be saved to the same file as TestIni, but not to be loaded from file. */
struct SavedIni: IniConfig
{
    SavedIni(): IniConfig(ini().iniFile()) {}

    NX_INI_STRING(nullptr, nullStr, "Null string."); //< Test the recovery from null values.

    NX_INI_FLAG(1, enableOutput, "Enable Output.");
    NX_INI_FLAG(0, enableTime, "Enable Time.");
    NX_INI_FLAG(0, enableFps, "Enable Fps.");
    NX_INI_STRING(" another string with leading space", str0, "String with leading space.");
    NX_INI_STRING("another string with middle \" quote", str1, "String with middle quote.");
    NX_INI_STRING("\" another string with leading quote", str2, "String with leading quote.");
    NX_INI_STRING("another string with trailing quote \"", str3, "String with trailing quote.");
    NX_INI_STRING("\"another enquoted string\"", str4, "Enquoted string.");
    NX_INI_STRING("another plain string", str5, "Plain string.");
    NX_INI_STRING("another plain string with \\ backslash", str6, "Plain string with backslash.");
    NX_INI_INT(777, intNumber, "Test int number.");
    NX_INI_FLOAT(0.432F, floatNumber ,"Test float number.");
};

template<class ExpectedIni, class ActualIni>
static void assertIniEquals(int line, const ExpectedIni& expected, const ActualIni& actual)
{
    #define ASSERT_INI_PARAM_EQ(PARAM) ASSERT_EQ_AT_LINE(line, expected.PARAM, actual.PARAM)
    #define ASSERT_INI_PARAM_STREQ(PARAM) ASSERT_STREQ_AT_LINE(line, expected.PARAM, actual.PARAM)

    ASSERT_INI_PARAM_EQ(enableOutput);
    ASSERT_INI_PARAM_EQ(enableTime);
    ASSERT_INI_PARAM_EQ(enableFps);
    ASSERT_INI_PARAM_STREQ(str0);
    ASSERT_INI_PARAM_STREQ(str1);
    ASSERT_INI_PARAM_STREQ(str2);
    ASSERT_INI_PARAM_STREQ(str3);
    ASSERT_INI_PARAM_STREQ(str4);
    ASSERT_INI_PARAM_STREQ(str5);
    ASSERT_INI_PARAM_STREQ(str6);
    ASSERT_INI_PARAM_EQ(intNumber);
    ASSERT_INI_PARAM_EQ(floatNumber);

    #undef ASSERT_INI_PARAM_EQ
    #undef ASSERT_INI_PARAM_STREQ
}

/**
 * Create a .ini file with the contents from the supplied IniConfig object.
 */
static void generateIniFile(const SavedIni& ini)
{
    std::ostringstream content;

    // C-style-escape the string if and only if it contains a quote or a leading space.
    const auto outputStr =
        [&content](const std::string& value, const char* paramName)
        {
            content << paramName << "=";
            if (value[0] == ' ' || value.find('"') != std::string::npos)
                content << nx::kit::utils::toString(value);
            else
                content << value;
            content << std::endl;
        };

    #define GENERATE_INI_PARAM_VAL(PARAM) \
        content << #PARAM << "=" << nx::kit::utils::toString(ini.PARAM) << std::endl
    #define GENERATE_INI_PARAM_STR(PARAM) outputStr(ini.PARAM, #PARAM)

    GENERATE_INI_PARAM_VAL(enableOutput);
    GENERATE_INI_PARAM_VAL(enableTime);
    GENERATE_INI_PARAM_VAL(enableFps);
    GENERATE_INI_PARAM_STR(str0);
    GENERATE_INI_PARAM_STR(str1);
    GENERATE_INI_PARAM_STR(str2);
    GENERATE_INI_PARAM_STR(str3);
    GENERATE_INI_PARAM_STR(str4);
    GENERATE_INI_PARAM_STR(str5);
    GENERATE_INI_PARAM_STR(str6);
    GENERATE_INI_PARAM_VAL(intNumber);
    GENERATE_INI_PARAM_VAL(floatNumber);

    #undef GENERATE_INI_PARAM_STR
    #undef GENERATE_INI_PARAM_VAL

    nx::kit::test::createFile(ini.iniFilePath(), content.str().c_str());
}

template<class Ini, class ExpectedIni>
static void testReload(
    int line,
    const ExpectedIni& expectedIni,
    Ini* ini,
    const std::string& testCaseTag,
    const std::string& expectedOutput)
{
    std::ostringstream output;
    IniConfig::setOutput(&output);

    ini->reload();

    IniConfig::setOutput(&std::cerr); //< Restore global setting.
    const std::string outputStr = output.str();

    if (IniConfig::isEnabled())
    {
        nx::kit::test::assertMultilineTextEquals(__FILE__, line, testCaseTag,
            expectedOutput, outputStr, ini->iniFilePath(), "{iniFilePath}");
    }
    else
    {
        ASSERT_TRUE_AT_LINE(line, outputStr.empty()); //< If disabled, there must be no output.
    }

    if (IniConfig::isEnabled())
        assertIniEquals(line, expectedIni, *ini);
    else
        assertIniEquals(line, defaultIni, *ini); //< If disabled, values never change.
}

TEST(iniConfig, testSuccess)
{
    std::cerr << "IniConfig::isEnabled() -> " << (IniConfig::isEnabled() ? "true" : "false")
        << std::endl;

    // Avoiding nx::kit::test::tempDir() for disabled ini_config to avoid dir name conflict.
    const char* const tempDir = IniConfig::isEnabled() ? nx::kit::test::tempDir() : "stub";
    IniConfig::setIniFilesDir(tempDir);
    ASSERT_STREQ(tempDir, IniConfig::iniFilesDir());

    // Check path properties of IniConfig.
    ASSERT_STREQ(kIniFile, ini().iniFile());
    ASSERT_STREQ(std::string(IniConfig::iniFilesDir()) + kIniFile, ini().iniFilePath());

    testReload(__LINE__, defaultIni, &ini(), "first reload(), .ini file not found, verbose",
        "test.ini (absent) To fill in defaults, touch {iniFilePath}\n");

    testReload(__LINE__, defaultIni, &ini(), ".ini file still not found, silent", "");

    // Create/delete ini file with different values.
    const SavedIni savedIni; //< Create on the stack to test this possibility.
    if (IniConfig::isEnabled())
        generateIniFile(savedIni);

    testReload(__LINE__, savedIni, &ini(), "values changed, verbose",
        /*suppress newline*/ 1 + (const char*)
R"(
test.ini [{iniFilePath}]
  * enableOutput=true
  * enableTime=false
  * enableFps=false
  * str0=" another string with leading space"
  * str1="another string with middle \" quote"
  * str2="\" another string with leading quote"
  * str3="another string with trailing quote \""
  * str4="\"another enquoted string\""
  * str5="another plain string"
  * str6="another plain string with \\ backslash"
  * intNumber=777
  * floatNumber=0.432
)");

    testReload(__LINE__, savedIni, &ini(), "values not changed, silent", "");
}

TEST(iniConfig, testParsingErrors)
{
    if (!IniConfig::isEnabled())
    {
        std::cerr << "IniConfig::isEnabled() -> false" << std::endl;
        return; //< Nothing to test if IniConfig is disabled at compile time.
    }

    IniConfig::setIniFilesDir(nx::kit::test::tempDir());
    TestIni ini;

    std::string content = /*suppress newline*/ 1 + (const char*)
R"(
unknownParam=value
unknownParamWithEmptyValue=
missingEqualsAfterName
=EmptyParamName
nullStr=zero-code character {NUL} inside
enableOutput=non-bool
enableTime=2
enableFps=1.0
str0="Enquoted string with invalid \* escape sequence"
str1="Missing closing quote
str2="Unexpected trailing " after the closing quote
str3="Non-printable {TAB} character (tab) in an enquoted string"
str4="Enquoted string with unsupported escape sequence \u"
str5="Octal escape sequence \666 does not fit in one byte"
str6="Hex escape sequence \xDEAD does not fit in one byte"
intNumber=3.14
floatNumber=non-number
)";
    nx::kit::utils::stringReplaceAll(&content, "{TAB}", "\x09");
    nx::kit::utils::stringReplaceAll(&content, "{NUL}", std::string("\0", 1));
    nx::kit::test::createFile(ini.iniFilePath(), content);

    testReload(__LINE__, defaultIni, &ini, "with errors, verbose",
        /*suppress newline*/ 1 + (const char*)
R"(
test.ini [{iniFilePath}]
test.ini ERROR: Missing "=" after the name "missingEqualsAfterName". Line 3, file {iniFilePath}
test.ini ERROR: The name part (before "=") is empty. Line 4, file {iniFilePath}
  ! enableOutput=false [invalid value in file]
  ! enableTime=true [invalid value in file]
  ! enableFps=true [invalid value in file]
  ERROR in the value of str0: Invalid escaped character '*' after the backslash.
  ! str0=" string with leading space" [invalid value in file]
  ERROR in the value of str1: Missing the closing quote.
  ! str1="string with middle \" quote" [invalid value in file]
  ERROR in the value of str2: Unexpected trailing after the closing quote.
  ! str2="\" string with leading quote" [invalid value in file]
  ERROR in the value of str3: Found non-printable ASCII character '\x09'.
  ! str3="string with trailing quote \"" [invalid value in file]
  ERROR in the value of str4: Invalid escaped character 'u' after the backslash.
  ! str4="\"enquoted string\"" [invalid value in file]
  ERROR in the value of str5: Octal escape sequence does not fit in one byte: 438.
  ! str5="plain string" [invalid value in file]
  ERROR in the value of str6: Hex escape sequence does not fit in one byte.
  ! str6="plain string with \\ backslash" [invalid value in file]
  ! intNumber=113 [invalid value in file]
  ! floatNumber=310.55 [invalid value in file]
  ! nullStr [unexpected param in file]
  ! unknownParam [unexpected param in file]
  ! unknownParamWithEmptyValue [unexpected param in file]
)");
}

static void testBom(const char* iniFileContentPrefix)
{
    if (!IniConfig::isEnabled())
    {
        std::cerr << "IniConfig::isEnabled() -> false" << std::endl;
        return; //< Nothing to test if IniConfig is disabled at compile time.
    }

    IniConfig::setIniFilesDir(nx::kit::test::tempDir());

    TestIni ini;

    TestIni expectedIni;
    const_cast<bool&>(expectedIni.enableOutput) = true;

    nx::kit::test::createFile(
        ini.iniFilePath(),
        std::string(iniFileContentPrefix) + "enableOutput=true");

    testReload(__LINE__, expectedIni, &ini, "testBom",
        /*suppress newline*/ 1 + (const char*)
R"(
test.ini [{iniFilePath}]
  * enableOutput=true
    enableTime=true
    enableFps=true
    str0=" string with leading space"
    str1="string with middle \" quote"
    str2="\" string with leading quote"
    str3="string with trailing quote \""
    str4="\"enquoted string\""
    str5="plain string"
    str6="plain string with \\ backslash"
    intNumber=113
    floatNumber=310.55
)");
}

TEST(iniConfig, testBomAtStartOfFirstLine)
{
    testBom("\xEF\xBB\xBF");
}

TEST(iniConfig, testBomAsFirstLine)
{
    testBom("\xEF\xBB\xBF\n");
}

TEST(iniConfig, testGetParamTypeAndValue)
{
    if (!IniConfig::isEnabled())
    {
        std::cerr << "IniConfig::isEnabled() -> false" << std::endl;
        return; //< Nothing to test if IniConfig is disabled at compile time.
    }

    TestIni ini;

    const void* data{nullptr};
    IniConfig::ParamType type{};

    ASSERT_TRUE(ini.getParamTypeAndValue("enableOutput", &type, &data));
    ASSERT_EQ(IniConfig::ParamType::boolean, type);
    ASSERT_EQ(false, *static_cast<const bool*>(data));

    ASSERT_TRUE(ini.getParamTypeAndValue("enableTime", &type, &data));
    ASSERT_EQ(IniConfig::ParamType::boolean, type);
    ASSERT_EQ(true, *static_cast<const bool*>(data));

    ASSERT_TRUE(ini.getParamTypeAndValue("intNumber", &type, &data));
    ASSERT_EQ(IniConfig::ParamType::integer, type);
    ASSERT_EQ(113, *static_cast<const int*>(data));

    ASSERT_TRUE(ini.getParamTypeAndValue("floatNumber", &type, &data));
    ASSERT_EQ(IniConfig::ParamType::float_, type);
    ASSERT_EQ(310.55F, *static_cast<const float*>(data));

    ASSERT_TRUE(ini.getParamTypeAndValue("str5", &type, &data));
    ASSERT_EQ(IniConfig::ParamType::string, type);
    ASSERT_STREQ("plain string", *static_cast<const char* const*>(data));

    ASSERT_FALSE(ini.getParamTypeAndValue("nonExistentParameter", &type, &data));
}
