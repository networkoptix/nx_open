// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <algorithm>
#include <climits>

#include <nx/kit/test.h>
#include <nx/kit/utils.h>

namespace nx {
namespace kit {
namespace utils {
namespace test {

TEST(utils, getProcessCmdLineArgs)
{
    const auto& args = getProcessCmdLineArgs();

    ASSERT_FALSE(args.empty());
    ASSERT_TRUE(args[0].find("nx_kit_ut") != std::string::npos);

    std::cerr << "Process has " << args.size() << " arg(s):" << std::endl;
    for (const auto& arg: args)
        std::cerr << nx::kit::utils::toString(arg) << std::endl;
}

TEST(utils, baseName)
{
    ASSERT_STREQ("", baseName(""));
    ASSERT_STREQ("", baseName("/"));
    ASSERT_STREQ("bar", baseName("/foo/bar"));
    ASSERT_STREQ("bar", baseName("foo/bar"));

    #if defined(_WIN32)
        ASSERT_STREQ("bar", baseName("d:bar"));
        ASSERT_STREQ("bar", baseName("X:\\bar"));
        ASSERT_STREQ("bar", baseName("X:/bar"));
    #endif
}

TEST(utils, absolutePath)
{
    ASSERT_STREQ("", absolutePath("", ""));
    ASSERT_STREQ("origin", absolutePath("origin", ""));
    ASSERT_STREQ("/x/y", absolutePath("", "/x/y"));

    ASSERT_STREQ("/x/y", absolutePath("ignored", "/x/y"));
    ASSERT_STREQ("x/y", absolutePath("", "x/y"));
    ASSERT_STREQ("origin/x/y", absolutePath("origin", "x/y"));
    ASSERT_STREQ("origin/x/y", absolutePath("origin/", "x/y"));

    #if defined(_WIN32)
        ASSERT_STREQ("\\x\\y", absolutePath("", "\\x\\y"));

        ASSERT_STREQ("C:\\x\\y", absolutePath("ignored", "C:\\x\\y"));
        ASSERT_STREQ("C:/x/y", absolutePath("ignored", "C:/x/y"));
        ASSERT_STREQ("x\\y", absolutePath("", "x\\y"));
        ASSERT_STREQ("\\x\\y", absolutePath("ignored", "\\x\\y"));
        ASSERT_STREQ("origin\\x/y", absolutePath("origin\\", "x/y"));
        ASSERT_STREQ("origin\\x\\y", absolutePath("origin", "x\\y"));
        ASSERT_STREQ("origin\\x\\y/z", absolutePath("origin", "x\\y/z"));

        ASSERT_STREQ("origin\\a\\path", absolutePath("origin\\a", "path"));
        ASSERT_STREQ("origin/a/path", absolutePath("origin/a", "path"));
    #endif
}

TEST(utils, getProcessName)
{
    const std::string processName = getProcessName();

    ASSERT_FALSE(processName.empty());
    ASSERT_TRUE(processName.find(".exe") == std::string::npos);
    ASSERT_TRUE(processName.find('/') == std::string::npos);
    ASSERT_TRUE(processName.find('\\') == std::string::npos);
}

TEST(utils, alignUp)
{
    ASSERT_EQ(0U, alignUp(0, 0));
    ASSERT_EQ(17U, alignUp(17, 0));
    ASSERT_EQ(48U, alignUp(48, 16));
    ASSERT_EQ(48U, alignUp(42, 16));
    ASSERT_EQ(7U, alignUp(7, 7));
    ASSERT_EQ(8U, alignUp(7, 8));
}

TEST(utils, misalignedPtr)
{
    uint8_t data[1024];
    const auto aligned = (uint8_t*) alignUp((intptr_t) data, 32);
    ASSERT_EQ(0, (intptr_t) aligned % 32);
    uint8_t* const misaligned = misalignedPtr(data);
    ASSERT_TRUE((intptr_t) misaligned % 32 != 0);
}

static void testDecodeEscapedString(
    int line,
    const std::string& expectedErrorMessage,
    const std::string& expectedResult,
    const std::string& encodedString)
{
    std::string actualErrorMessage;
    const std::string actualResult = decodeEscapedString(encodedString, &actualErrorMessage);

    // Print the values of interest on failure.
    if (expectedResult != actualResult || expectedErrorMessage != actualErrorMessage)
    {
        std::cerr << "Encoded test string: [" << encodedString << "]" << std::endl;
        std::cerr << "Actual error message: [" << actualErrorMessage << "]" << std::endl;
    }

    ASSERT_STREQ_AT_LINE(line, expectedResult, actualResult);
    ASSERT_STREQ_AT_LINE(line, expectedErrorMessage, actualErrorMessage);
}

TEST(utils, decodeEscapedStringSuccess)
{
    #define TEST_SUCCESS(EXPECTED_RESULT, ENCODED_STRING) \
        testDecodeEscapedString(__LINE__, /*expectedErrorMessage*/ "", \
            /* Construct an std::string from a char array possibly containing '\0' inside. */ \
            std::string(EXPECTED_RESULT, sizeof(EXPECTED_RESULT) - /*terminating \0*/ 1), \
            ENCODED_STRING)

    TEST_SUCCESS("", R"("")");
    TEST_SUCCESS("abc", R"("abc")");
    TEST_SUCCESS("\" quote", R"("\" quote")");
    TEST_SUCCESS("\\ backslash", R"("\\ backslash")");
    TEST_SUCCESS("' apostrophe", R"("\' apostrophe")");
    TEST_SUCCESS("? escaped question mark", R"("\? escaped question mark")");
    TEST_SUCCESS("? unescaped question mark", R"("? unescaped question mark")");
    TEST_SUCCESS("\a bel", R"("\a bel")");
    TEST_SUCCESS("\b bs", R"("\b bs")");
    TEST_SUCCESS("\f ff", R"("\f ff")");
    TEST_SUCCESS("\n lf", R"("\n lf")");
    TEST_SUCCESS("\r cr", R"("\r cr")");
    TEST_SUCCESS("\t tab", R"("\t tab")");
    TEST_SUCCESS("\v vt", R"("\v vt")");

    TEST_SUCCESS("octal \0 after nul", R"("octal \0 after nul")");
    TEST_SUCCESS("\1 octal", R"("\1 octal")");
    TEST_SUCCESS("\00 octal", R"("\00 octal")");
    TEST_SUCCESS("\000 octal", R"("\000 octal")");
    TEST_SUCCESS("w octal", R"("\167 octal")"); //< 0167 (octal) is the ASCII code of `w`.
    TEST_SUCCESS("\001""234", R"("\001234")"); //< Octal sequence takes up to 3 digits.

    TEST_SUCCESS("hex \0 after nul", R"("hex \x0 after nul")");
    TEST_SUCCESS("\xFF", R"("\xFF")");
    TEST_SUCCESS("\xAb", R"("\xAb")");
    TEST_SUCCESS("@", R"("\x40")"); //< ASCII code of '@' is 0x40.
    TEST_SUCCESS("\xFF", R"("\x000FF")"); //< Hex sequence takes as much digits as are present.
    TEST_SUCCESS("pre \xF post", R"("pre \xF post")");

    TEST_SUCCESS("\xFE non-ascii", R"("\xFE"" non-ascii")");

    // Concatenation of consecutive enquoted literals.
    TEST_SUCCESS("\xFE non-ascii", R"("\xFE"" non-ascii")"); //< No whitespace between the quotes.
    TEST_SUCCESS("abc123", R"("abc" "123")"); //< A space between the quotes.
    TEST_SUCCESS("abc123", "\"abc\" \n \"123\""); //< Various whitespace chars between the quotes.

    #undef TEST_SUCCESS
}

TEST(utils, decodeEscapedStringFailure)
{
    #define TEST_FAILURE(EXPECTED_ERROR_MESSAGE, EXPECTED_RESULT, S) \
        testDecodeEscapedString(__LINE__, EXPECTED_ERROR_MESSAGE, EXPECTED_RESULT, S)

    // NOTE: `R"(` is not used because invalid escape sequences in it do not compile in MSVC.

    TEST_FAILURE("Found non-printable ASCII character '\\x0A'.",
        "a_\n_b", "\"a_\n_b\"");
    TEST_FAILURE("The string does not start with a quote.",
        "", "");
    TEST_FAILURE("The string does not start with a quote.",
        "abc\"", "abc\"");
    TEST_FAILURE("Missing the closing quote.",
        "", "\"");
    TEST_FAILURE("Missing the closing quote.",
        "abc", "\"abc");
    TEST_FAILURE("Invalid escaped character 'z' after the backslash.",
        "_z_", "\"_\\z_\"");
    TEST_FAILURE("Unexpected trailing after the closing quote.",
        "str_trailing", "\"str\"_trailing");

    // Unicode escape sequences are not supported.
    TEST_FAILURE("Invalid escaped character 'u' after the backslash.",
        "_uFFFF_", "\"_\\uFFFF_\"");
    TEST_FAILURE("Invalid escaped character 'U' after the backslash.",
        "_Uabcd_", "\"_\\Uabcd_\"");

    // Test accumulating multiple errors, with messages concatenated via a space.
    TEST_FAILURE(
        "Missing escaped character after the backslash."
        " "
        "Missing the closing quote.",
        "abc\\", "\"abc\\");
    TEST_FAILURE(
        "Invalid escaped character 'z' after the backslash."
        " "
        "Found non-printable ASCII character '\\x0D'."
        " "
        "Missing the closing quote.",
        "_z_\r_", "\"_\\z_\r_");

    TEST_FAILURE("Octal escape sequence does not fit in one byte: 511.",
        "\xFF", "\"\\777\""); //< 0777 (octal) = 0x1FF = 511.

    TEST_FAILURE("Hex escape sequence does not fit in one byte.",
        "@", "\"\\xEE40\""); //, ASCII code of '@' is 0x40.
    TEST_FAILURE("Missing hex digits in the hex escape sequence.",
        "a__b", "\"a_\\x_b\"");

    #undef TEST_FAILURE
}

TEST(utils, toString_string)
{
    ASSERT_STREQ("\"abc\"", toString(/*rvalue*/ std::string("abc")));

    std::string nonConstString = "abc";
    ASSERT_STREQ("\"abc\"", toString(nonConstString));

    const std::string constString = "abc";
    ASSERT_STREQ("\"abc\"", toString(constString));

    std::string strWithNulInside = "_Z_";
    strWithNulInside[1] = '\0';
    ASSERT_STREQ("\"_\\000_\"", toString(strWithNulInside));
}

TEST(utils, toString_wstring)
{
    ASSERT_STREQ("\"abc\"", toString(/*rvalue*/ std::wstring(L"abc")));

    std::wstring nonConstWstring = L"abc";
    ASSERT_STREQ("\"abc\"", toString(nonConstWstring));

    const std::wstring constWstring = L"abc";
    ASSERT_STREQ("\"abc\"", toString(constWstring));

    std::wstring strWithNulInside = L"_Z_";
    strWithNulInside[1] = '\0';
    ASSERT_STREQ("\"_\\000_\"", toString(strWithNulInside));
}

TEST(utils, toString_ptr)
{
    ASSERT_STREQ("null", toString((const void*) nullptr));
    ASSERT_STREQ("null", toString(nullptr));

    const int dummyInt = 42;
    char expectedS[100];
    snprintf(expectedS, sizeof(expectedS), "%p", &dummyInt);
    ASSERT_EQ(expectedS, toString(&dummyInt));
}

TEST(utils, toString_bool)
{
    ASSERT_STREQ("true", toString(true));
    ASSERT_STREQ("false", toString(false));
}

TEST(utils, toString_number)
{
    ASSERT_STREQ("42", toString(42));
    ASSERT_STREQ("3.14", toString(3.14));

    ASSERT_STREQ("42", toString((uint8_t) 42));
    ASSERT_STREQ("42", toString((int8_t) 42));
    ASSERT_STREQ("42", toString((uint16_t) 42));
    ASSERT_STREQ("42", toString((int16_t) 42));
}

TEST(utils, toString_char)
{
    ASSERT_STREQ("'C'", toString('C'));
    ASSERT_STREQ(R"('\'')", toString('\''));
    ASSERT_STREQ(R"('\xFF')", toString('\xFF'));
}

int kWcharSize = sizeof(wchar_t); //< Not const to suppress the warning about a constant condition.

TEST(utils, toString_wchar)
{
    ASSERT_STREQ("'C'", toString(L'C'));
    ASSERT_STREQ(R"('\'')", toString(L'\''));

    if (kWcharSize == 2) //< E.g. MSVC
    {
        ASSERT_STREQ(R"('\u00FF')", toString(L'\xFF'));
        ASSERT_STREQ(R"('\uABCD')", toString(L'\uABCD'));
    }
    else if (kWcharSize == 4) //< E.g. GCC/Clang
    {
        ASSERT_STREQ(R"('\U000000FF')", toString(L'\xFF'));
        ASSERT_STREQ(R"('\U0000ABCD')", toString(L'\uABCD'));
    }
}

TEST(utils, toString_char_ptr)
{
    ASSERT_STREQ(R"("str")", toString("str"));

    char nonConstChars[] = "str";
    ASSERT_STREQ(R"("str")", toString(nonConstChars));

    ASSERT_STREQ(R"("")", toString(""));

    // NOTE: `R"(` is not used here because `\"` does not compile in MSVC.
    ASSERT_STREQ("\"str\\\"with_quote\"", toString("str\"with_quote"));

    ASSERT_STREQ(R"("str\\with_backslash")", toString("str\\with_backslash"));
    ASSERT_STREQ(R"("str\twith_tab")", toString("str\twith_tab"));
    ASSERT_STREQ(R"("str\nwith_newline")", toString("str\nwith_newline"));
    ASSERT_STREQ(R"("str\rwith_cr")", toString("str\rwith_cr"));
    ASSERT_STREQ(R"("str\x7F""with_127")", toString("str\x7Fwith_127"));
    ASSERT_STREQ(R"("str\x1F""with_31")", toString("str\x1Fwith_31"));
    ASSERT_STREQ(R"("str\xFF""with_255")", toString("str\xFFwith_255"));
}

TEST(utils, toString_wchar_ptr)
{
    ASSERT_STREQ(R"("str")", toString(L"str"));

    wchar_t nonConstWchars[] = L"str";
    ASSERT_STREQ(R"("str")", toString(nonConstWchars));

    // NOTE: For '\u' and '\U', a code point is required other than a printable ASCII.
    if (kWcharSize == 2) //< MSVC
        ASSERT_STREQ(R"("-\u007F-")", toString(L"-\u007F-"));
    else if (kWcharSize == 4) //< Linux
        ASSERT_STREQ(R"("-\U0000007F-")", toString(L"-\U0000007F-"));
}

TEST(utils, fromString_int)
{
    int value = 0;

    ASSERT_TRUE(fromString("-3", &value));
    ASSERT_EQ(-3, value);

    ASSERT_TRUE(fromString("42", &value));
    ASSERT_EQ(42, value);

    ASSERT_TRUE(fromString(toString(INT_MAX), &value));
    ASSERT_EQ(INT_MAX, value);

    ASSERT_TRUE(fromString(toString(INT_MIN), &value));
    ASSERT_EQ(INT_MIN, value);

    ASSERT_FALSE(fromString(/* INT_MAX * 10 */ toString(INT_MAX) + "0", &value));
    ASSERT_FALSE(fromString(/* INT_MIN * 10 */ toString(INT_MIN) + "0", &value));

    ASSERT_FALSE(fromString("text", &value));
    ASSERT_FALSE(fromString("42-some-suffix", &value));
    ASSERT_FALSE(fromString("2.0", &value));
    ASSERT_FALSE(fromString("", &value));
}

TEST(utils, fromString_double)
{
    double value = 0;

    ASSERT_TRUE(fromString("-3", &value));
    ASSERT_EQ(-3.0, value);

    ASSERT_TRUE(fromString("42", &value));
    ASSERT_EQ(42.0, value);

    ASSERT_TRUE(fromString("3.14", &value));
    ASSERT_EQ(3.14, value);

    ASSERT_FALSE(fromString("text", &value));
    ASSERT_FALSE(fromString("42-some-suffix", &value));
    ASSERT_FALSE(fromString("", &value));
}

TEST(utils, fromString_float)
{
    float value = 0;

    ASSERT_TRUE(fromString("-3", &value));
    ASSERT_EQ(-3.0F, value);

    ASSERT_TRUE(fromString("42", &value));
    ASSERT_EQ(42.0F, value);

    ASSERT_TRUE(fromString("3.14", &value));
    ASSERT_EQ(3.14F, value);

    ASSERT_FALSE(fromString("text", &value));
    ASSERT_FALSE(fromString("42-some-suffix", &value));
    ASSERT_FALSE(fromString("", &value));
}

TEST(utils, fromString_bool)
{
    bool value = true;

    ASSERT_TRUE(fromString("false", &value));
    ASSERT_FALSE(value);

    ASSERT_TRUE(fromString("true", &value));
    ASSERT_TRUE(value);

    ASSERT_TRUE(fromString("False", &value));
    ASSERT_FALSE(value);

    ASSERT_TRUE(fromString("True", &value));
    ASSERT_TRUE(value);

    ASSERT_TRUE(fromString("FALSE", &value));
    ASSERT_FALSE(value);

    ASSERT_TRUE(fromString("TRUE", &value));
    ASSERT_TRUE(value);

    ASSERT_FALSE(fromString("tRuE", &value));
    ASSERT_FALSE(fromString("2", &value));
    ASSERT_FALSE(fromString("", &value));
    ASSERT_FALSE(fromString("xxx", &value));
}

static void testStringReplaceAllChars(
    const std::string& expected, const std::string& source, char sample, char replacement)
{
    std::string s = source;
    stringReplaceAllChars(&s, sample, replacement);
    ASSERT_EQ(expected, s);
}

TEST(utils, stringReplaceAllChars)
{
    testStringReplaceAllChars("", "", 'a', 'b');
    testStringReplaceAllChars("1b2", "1a2", 'a', 'b'); //< 'Replace each 'a' with 'b'.
    testStringReplaceAllChars("$", "\n", '\n', '$'); //< Replace each '\n' with '$'.
}

static void testStringInsertAfterEach(
    const std::string& expected, const std::string& source, char sample, const char* insertion)
{
    std::string s = source;
    stringInsertAfterEach(&s, sample, insertion);
    ASSERT_EQ(expected, s);
}

TEST(utils, stringInsertAfterEach)
{
    testStringInsertAfterEach("", "", 'a', "123");
    testStringInsertAfterEach("a123ba123", "aba", 'a', "123"); //< Insert '123' after each 'a'.
    testStringInsertAfterEach("a\n$", "a\n", '\n', "$"); //< Insert '$' after each '\n'.
}

static void testStringReplaceAll(
    const std::string& expected,
    const std::string& source,
    const std::string& sample,
    const std::string& replacement)
{
    std::string s = source;
    stringReplaceAll(&s, sample, replacement);
    ASSERT_EQ(expected, s);
}

TEST(utils, stringReplaceAll)
{
    testStringReplaceAll("", "", "ab", "123");
    testStringReplaceAll("a45b45", "a23b23", "23", "45");
    testStringReplaceAll("45", "23", "23", "45");
    testStringReplaceAll("x", "x", "23", "45");
    testStringReplaceAll("xy", "x23y", "23", "");
    testStringReplaceAll("", "23", "23", "");

    // Test support for '\0' inside the strings.
    #define STR(LITERAL) std::string(LITERAL, sizeof(LITERAL) - /*terminating \0*/ 1)
    testStringReplaceAll(STR("ac\0z"), STR("a\0bz"), STR("\0b"), STR("c\0"));
    #undef STR
}

TEST(utils, stringStartsWith)
{
    ASSERT_TRUE(stringStartsWith("abc", "a"));
    ASSERT_TRUE(stringStartsWith("abc", "ab"));
    ASSERT_TRUE(stringStartsWith("abc", "abc"));
    ASSERT_TRUE(stringStartsWith("abc", ""));
    ASSERT_TRUE(stringStartsWith("", ""));

    ASSERT_FALSE(stringStartsWith("", "a"));
    ASSERT_FALSE(stringStartsWith("b", "a"));
    ASSERT_FALSE(stringStartsWith("abc", "X"));
    ASSERT_FALSE(stringStartsWith("abc", "c"));
    ASSERT_FALSE(stringStartsWith("abc", "abX"));
    ASSERT_FALSE(stringStartsWith("a", "a_longer_prefix"));
}

TEST(utils, stringEndsWith)
{
    ASSERT_TRUE(stringEndsWith("abc", "c"));
    ASSERT_TRUE(stringEndsWith("abc", "bc"));
    ASSERT_TRUE(stringEndsWith("abc", "abc"));
    ASSERT_TRUE(stringEndsWith("abc", ""));
    ASSERT_TRUE(stringEndsWith("", ""));

    ASSERT_FALSE(stringEndsWith("", "a"));
    ASSERT_FALSE(stringEndsWith("b", "a"));
    ASSERT_FALSE(stringEndsWith("abc", "X"));
    ASSERT_FALSE(stringEndsWith("abc", "a"));
    ASSERT_FALSE(stringEndsWith("abc", "Xbc"));
    ASSERT_FALSE(stringEndsWith("x", "a_longer_suffix"));
}

TEST(utils, trimString)
{
    ASSERT_EQ("abc", trimString("abc"));
    ASSERT_EQ("a", trimString("a"));
    ASSERT_EQ("", trimString(""));
    ASSERT_EQ("a", trimString(" a"));
    ASSERT_EQ("a  b", trimString("  a  b"));
    ASSERT_EQ("a", trimString("a "));
    ASSERT_EQ("a  b", trimString("a  b  "));
    ASSERT_EQ("a\nb", trimString("\na\nb"));
    ASSERT_EQ("a\nb", trimString("a\nb\n\n"));
}

TEST(utils, isSpaceOrControlChar)
{
    for (char c = 0; c <= 32; ++c)
    {
        ASSERT_TRUE(isSpaceOrControlChar(c));
    }
    ASSERT_TRUE(isSpaceOrControlChar(127));
    ASSERT_FALSE(isSpaceOrControlChar('a'));
    ASSERT_FALSE(isSpaceOrControlChar('A'));
    ASSERT_FALSE(isSpaceOrControlChar('\x9C'));
    ASSERT_FALSE(isSpaceOrControlChar('\xEA'));
}

TEST(utils, toUpper)
{
    ASSERT_EQ("", toUpper(""));
    ASSERT_EQ("ABCI", toUpper("abci"));
    ASSERT_EQ("DFG", toUpper("DFG"));
    // The following string contains escaped characters for the plus-minus sign.
    ASSERT_EQ("ABC \xB1", toUpper("abc \xB1"));
}

} // namespace test
} // namespace utils
} // namespace kit
} // namespace nx
