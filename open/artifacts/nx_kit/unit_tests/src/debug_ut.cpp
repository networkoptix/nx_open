// Copyright 2018-present Network Optix, Inc.

#include <algorithm>
#include <cstring>

#include <nx/kit/test.h>
#include <nx/kit/debug.h>

#undef NX_DEBUG_INI
#define NX_DEBUG_INI ini.

namespace nx {
namespace kit {
namespace debug {
namespace test {

TEST(debug, enabledOutput)
{
    static constexpr struct
    {
        const bool enableOutput = true;
    } ini{};

    bool sideEffect = false;
    NX_OUTPUT << "NX_OUTPUT - enabled output, side effect:" << (sideEffect = true);
    ASSERT_TRUE(sideEffect);

    sideEffect = false;
    LL NX_PRINT << "LL NX_PRINT, side effect: " << (sideEffect = true);
    ASSERT_TRUE(sideEffect);
}

TEST(debug, disabledOutput)
{
    static constexpr struct
    {
        const bool enableOutput = false;
    } ini{};

    bool sideEffect = false;
    NX_OUTPUT << "NX_OUTPUT - disabled output, side effect:" << (sideEffect = true);
    ASSERT_FALSE(sideEffect);

    sideEffect = false;
    LL NX_PRINT << "LL NX_PRINT, side effect: " << (sideEffect = true);
    ASSERT_TRUE(sideEffect);
}

TEST(debug, overrideStreamStatic)
{
    std::ostringstream stringStream;

    std::ostream* const oldStream = stream();
    stream() = &stringStream;

    NX_PRINT << "TEST";

    stream() = oldStream;
    ASSERT_EQ("[" + fileBaseNameWithoutExt(__FILE__) + "] TEST\n", stringStream.str());
}

TEST(debug, overrideStreamPreproc)
{
    // The stream will be referenced from a lambda, thus, a static variable is needed.
    static std::ostream* stream = nullptr;
    std::ostringstream stringStream;
    stream = &stringStream;

    #undef NX_DEBUG_STREAM
    #define NX_DEBUG_STREAM *stream

    NX_PRINT << "TEST";

    // Restore default stream.
    #undef NX_DEBUG_STREAM
    #define NX_DEBUG_STREAM *stream()

    ASSERT_EQ("[" + fileBaseNameWithoutExt(__FILE__) + "] TEST\n", stringStream.str());
}

TEST(debug, assertSuccess)
{
    static const bool trueCondition = true;
    NX_KIT_ASSERT(trueCondition);
    NX_KIT_ASSERT(trueCondition, "This assertion with a message should not fail.");
}

TEST(debug, assertFailureInRelease)
{
    #if defined(NDEBUG)
        static const bool condition = false;
    #else
        static const bool condition = true;
    #endif

    NX_KIT_ASSERT(condition, "This and the next assertions should fail in Debug.");
    NX_KIT_ASSERT(condition);
}

TEST(debug, printValue)
{
    const char charValue = 'z';
    NX_PRINT_VALUE(charValue);

    const int constInt = 42;
    NX_PRINT_VALUE(constInt);
    NX_PRINT_VALUE(&constInt);

    const int* const nullPtr = nullptr;
    NX_PRINT_VALUE(nullPtr);

    const char* const constStr = "const char*";
    NX_PRINT_VALUE(constStr);

    const char* const nullStr = nullptr;
    NX_PRINT_VALUE(nullStr);

    char nonConstStr[] = "char*: b=\\ q=\" n=\n t=\t e=\033 7F=\x7F 80=\x80";
    NX_PRINT_VALUE(nonConstStr);

    const std::string stdString = "std::string";
    NX_PRINT_VALUE(stdString);
}

TEST(debug, hexDump)
{
    char data[257];
    for (size_t i = 0; i < sizeof(data); ++i)
        data[i] = (char) i;
    data[256] = '@'; //< To print incomplete last line of a multi-line hex dump.

    NX_PRINT_HEX_DUMP("&data[0]", &data[0], sizeof(&data[0])); //< Short hex dump.

    NX_PRINT_HEX_DUMP("data", &data, sizeof(data)); //< Long hex dump.
}

TEST(debug, enabledFps)
{
    static constexpr struct
    {
        const bool enableFpsTestTag = true;
    } ini{};

    for (int i = 0; i < 3; ++i)
        NX_FPS(TestTag, "fps-extra");
}

TEST(debug, disabledFps)
{
    static constexpr struct
    {
        const bool enableFpsTestTag = false;
    } ini{};

    for (int i = 0; i < 3; ++i)
        NX_FPS(TestTag, "fps-extra");
}

TEST(debug, enabledTime)
{
    static constexpr struct
    {
        const bool enableTime = true;
    } ini{};

    NX_TIME_BEGIN(timeNoMark);
    NX_TIME_END(timeNoMark);

    NX_TIME_BEGIN(timeMark);
    NX_TIME_MARK(timeMark, "t1");
    NX_TIME_MARK(timeMark, "t2");
    NX_TIME_END(timeMark);
}

TEST(debug, disabledTime)
{
    static constexpr struct
    {
        const bool enableTime = false;
    } ini{};

    NX_TIME_BEGIN(testTag);
    NX_TIME_END(testTag);
}

/**
 * Call pathSeparator(), converting its argument to use platform-dependent
 * slashes, and its result to use forward slashes.
 * @param file Path with either back or forward slashes.
 */
static std::string relativeSrcFilenameTest(const std::string& file)
{
    ASSERT_TRUE(pathSeparator() != '\0');

    std::string platformDependentFile = file;
    std::replace(platformDependentFile.begin(), platformDependentFile.end(),
        '/', pathSeparator());

    const char* const filePtr = platformDependentFile.c_str();
    const char* r = relativeSrcFilename(filePtr);

    // Test that the returned pointer points inside the string supplied to the function.
    bool pointsInside = false;
    for (const char* p = filePtr; *p != '\0'; ++p)
    {
        if (p == r)
        {
            pointsInside = true;
            break;
        }
    }
    ASSERT_TRUE(pointsInside);

    std::string result = r;
    std::replace(result.begin(), result.end(), pathSeparator(), '/');
    return result;
}

static bool stringEndsWithSuffix(const std::string& str, const std::string& suffix)
{
    if (str.length() < suffix.length())
        return false;
    return (str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0);
}

static std::string thisFileFolder()
{
    ASSERT_TRUE(pathSeparator() != '\0');
    const char* const file = __FILE__;
    const char* const separator2 = strrchr(file, pathSeparator());
    ASSERT_TRUE(separator2 != nullptr);
    const char* afterSeparator1 = separator2;
    while (afterSeparator1 > file && *(afterSeparator1 - 1) != pathSeparator())
        --afterSeparator1;
    // afterSeparator1 points after the previous separator or at the beginning of file.
    return std::string(afterSeparator1, separator2 - afterSeparator1);
}

static std::string thisFileExt()
{
    const char* const dot = strrchr(__FILE__, '.');
    if (!dot)
        return "";
    else
        return std::string(dot + 1);
}

TEST(debug, relativeSrcFilename)
{
    static const std::string kIrrelevantPath = "irrelevant/path/and/file.cpp";
    ASSERT_EQ(kIrrelevantPath, relativeSrcFilenameTest(kIrrelevantPath));

    static const std::string kAnyEnding = "nx/any/ending/file.cpp";
    ASSERT_EQ(kAnyEnding, relativeSrcFilenameTest("/any/starting/src/" + kAnyEnding));

    // debug.cpp: <commonPrefix>src/nx/kit/debug.cpp
    // __FILE__:  <commonPrefix>unit_tests/src/debug_ut.cpp
    //            <commonPrefix>toBeOmitted/dir/file.cpp

    std::string thisFile = __FILE__;
    std::replace(thisFile.begin(), thisFile.end(), pathSeparator(), '/');

    static const std::string suffix =
        thisFileFolder() + "/" + fileBaseNameWithoutExt(__FILE__) + "." + thisFileExt();
    ASSERT_TRUE(stringEndsWithSuffix(thisFile, suffix));

    const std::string commonPrefix = std::string(thisFile, 0, thisFile.size() - suffix.size());
    ASSERT_EQ("dir/file.cpp", relativeSrcFilenameTest(commonPrefix + "dir/file.cpp"));
}

} // namespace test
} // namespace debug
} // namespace kit
} // namespace nx
