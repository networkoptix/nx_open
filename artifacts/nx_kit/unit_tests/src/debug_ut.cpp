// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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

    // Testing that the code below does not produce warnings, namely about "dangling else".
    if (!sideEffect) //< ATTENTION: No braces should be written around NX_OUTPUT inside this `if`.
        NX_OUTPUT << "Should not be executed.";
}

TEST(debug, overrideStreamStatic)
{
    std::ostringstream stringStream;

    std::ostream* const oldStream = stream();
    stream() = &stringStream;

    NX_PRINT << "TEST";

    stream() = oldStream;
    ASSERT_EQ("[debug_ut] TEST\n", stringStream.str());
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

    ASSERT_EQ("[debug_ut] TEST\n", stringStream.str());
}

// TODO: Rework unit tests to check the captured actual output.

TEST(debug, assertSuccess)
{
    static const bool trueCondition = true;

    NX_KIT_ASSERT(trueCondition); //< Test NX_KIT_ASSERT() without message.
    NX_KIT_ASSERT(trueCondition,
        "This assertion with a message should not fail."); //< Test NX_KIT_ASSERT() with message.

    // Test NX_KIT_ASSERT() without message inside `if`.
    if (!NX_KIT_ASSERT(trueCondition))
        ASSERT_TRUE(false);

    // Test NX_KIT_ASSERT() with message inside `if`.
    if (!NX_KIT_ASSERT(trueCondition, "This assertion should not fail."))
        ASSERT_TRUE(false);
}

/**
 * Tests that assertions fail when they should, but tests it only in Release, because there is no
 * way to test assertion failures when built in Debug.
 */
TEST(debug, assertFailureInRelease)
{
    #if defined(NDEBUG)
        static const bool isDebug = false;
    #else
        static const bool isDebug = true;
    #endif

    NX_PRINT << "isDebug: " << nx::kit::utils::toString(isDebug);

    const char shouldFailInDebug[] = "This and the next assertion should fail in Release.";
    NX_KIT_ASSERT(isDebug, shouldFailInDebug); //< Test NX_KIT_ASSERT with message.
    NX_KIT_ASSERT(isDebug); //< Test NX_KIT_ASSERT without message.

    //< Test NX_KIT_ASSERT() with message inside `if`.
    if (NX_KIT_ASSERT(isDebug, shouldFailInDebug))
        ASSERT_TRUE(isDebug);
    else
        ASSERT_FALSE(isDebug);

    //< Test NX_KIT_ASSERT() without message inside `if`.
    if (NX_KIT_ASSERT(isDebug))
        ASSERT_TRUE(isDebug);
    else
        ASSERT_FALSE(isDebug);
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

TEST(debug, srcFileBaseNameWithoutExt)
{
    static const std::string s(1, nx::kit::utils::kPathSeparator);    
    
    ASSERT_EQ("", srcFileBaseNameWithoutExt(""));
    ASSERT_EQ("", srcFileBaseNameWithoutExt(s));
    ASSERT_EQ("", srcFileBaseNameWithoutExt("path" + s));

    ASSERT_EQ("name-no-ext", srcFileBaseNameWithoutExt("name-no-ext"));
    ASSERT_EQ("name-no-ext", srcFileBaseNameWithoutExt(s + "name-no-ext"));
    ASSERT_EQ("name-no-ext", srcFileBaseNameWithoutExt("path" + s + "name-no-ext"));

    ASSERT_EQ("name", srcFileBaseNameWithoutExt("name.ext"));
    ASSERT_EQ("name", srcFileBaseNameWithoutExt(s + "name.ext"));
    ASSERT_EQ("name", srcFileBaseNameWithoutExt("path" + s + "name.ext"));

    ASSERT_EQ("name", srcFileBaseNameWithoutExt("dir.bak" + s + "name.ext"));
    ASSERT_EQ("name", srcFileBaseNameWithoutExt("dir1" + s + "dir2" + s + "name.ext"));
    ASSERT_EQ("name.ext1", srcFileBaseNameWithoutExt("path" + s + "name.ext1.ext2"));
}

TEST(debug, srcFileRelativePath)
{
    static const std::string s(1, nx::kit::utils::kPathSeparator);

    static const std::string kIrrelevantPath = "irrelevant" + s + "path" + s + "file.cpp";
    ASSERT_EQ(kIrrelevantPath, srcFileRelativePath(kIrrelevantPath));

    // Test that if a path contains `/src/nx/`, it's relative path starts with that `nx/`.
    static const std::string kNxEnding = "nx" + s + "any_ending" + s + "file.cpp";
    ASSERT_EQ(kNxEnding, srcFileRelativePath(s + "any_starting" + s + "src" + s + kNxEnding));

    // debug.cpp: <commonPrefix>src/nx/kit/debug.cpp
    // __FILE__:  <commonPrefix>unit_tests/src/debug_ut.cpp

    const std::string thisFile = __FILE__;
    
    static const std::string suffix = "unit_tests" + s + "src" + s + "debug_ut.cpp";
    ASSERT_EQ(suffix, thisFile.substr(thisFile.size() - suffix.size(), suffix.size()));

    const std::string commonPrefix = std::string(thisFile, 0, thisFile.size() - suffix.size());
    ASSERT_EQ("dir" + s + "file.cpp", srcFileRelativePath(commonPrefix + "dir" + s + "file.cpp"));
}

} // namespace test
} // namespace debug
} // namespace kit
} // namespace nx
