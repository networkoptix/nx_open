// Copyright 2017 Network Optix, Inc. Licensed under GNU Lesser General Public License version 3.

#include <algorithm>
#include <cstring>

#include <nx/kit/test.h>
#include <nx/kit/debug.h>

using namespace nx::kit;

#undef NX_DEBUG_INI
#define NX_DEBUG_INI ini.

static std::string thisFileFolder()
{
    ASSERT_TRUE(debug::pathSeparator());
    const char* const file = __FILE__;
    const char* const separator2 = strrchr(file, debug::pathSeparator());
    ASSERT_TRUE(separator2 != nullptr);
    const char* afterSeparator1 = separator2;
    while (afterSeparator1 > file && *(afterSeparator1 - 1) != debug::pathSeparator())
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

TEST(debug, unalignedPtr)
{
    char data[1024];
    uint8_t* ptr = debug::unalignedPtr(&data);
    intptr_t ptrInt = (intptr_t) ptr;
    ASSERT_TRUE(ptrInt % 32 != 0);
}

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

    std::ostream* const oldStream = debug::stream();
    debug::stream() = &stringStream;

    NX_PRINT << "TEST";

    debug::stream() = oldStream;
    ASSERT_EQ("[" + debug::fileBaseNameWithoutExt(__FILE__) + "] TEST\n", stringStream.str());
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
    #define NX_DEBUG_STREAM *nx::kit::debug::stream()

    ASSERT_EQ("[" + debug::fileBaseNameWithoutExt(__FILE__) + "] TEST\n", stringStream.str());
}

TEST(debug, show)
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
 * Call debug::pathSeparator(), converting its argument to use platform-dependent
 * slashes, and its result to use forward slashes.
 * @param file Path with either back or forward slashes.
 */
static std::string relativeSrcFilename(const std::string& file)
{
    ASSERT_TRUE(debug::pathSeparator());

    std::string platformDependentFile = file;
    std::replace(platformDependentFile.begin(), platformDependentFile.end(),
        '/', debug::pathSeparator());

    const char* const filePtr = platformDependentFile.c_str();
    const char* r = debug::relativeSrcFilename(filePtr);

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
    std::replace(result.begin(), result.end(),
        debug::pathSeparator(), '/');
    return r;
}

static bool stringEndsWithSuffix(const std::string& str, const std::string& suffix)
{
    if (str.length() < suffix.length())
        return false;
    return (str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0);
}

TEST(debug, relativeSrcFilename)
{
    static const std::string kIrrelevantPath = "irrelevant/path/and/file.cpp";
    ASSERT_EQ(kIrrelevantPath, relativeSrcFilename(kIrrelevantPath));

    static const std::string kAnyEnding = "nx/any/ending/file.cpp";
    ASSERT_EQ(kAnyEnding, relativeSrcFilename("/any/starting/src/" + kAnyEnding));

    // debug.cpp: <commonPrefix>src/nx/kit/debug.cpp
    // __FILE__:  <commonPrefix>unit_tests/debug_ut.cpp
    //            <commonPrefix>toBeOmitted/dir/file.cpp

    std::string thisFile = __FILE__;
    std::replace(thisFile.begin(), thisFile.end(),
        debug::pathSeparator(), '/');

    static const std::string suffix =
        thisFileFolder() + "/" + debug::fileBaseNameWithoutExt(__FILE__) + "." + thisFileExt();
    NX_PRINT_VALUE(thisFileFolder());
    NX_PRINT_VALUE(suffix);
    ASSERT_TRUE(stringEndsWithSuffix(thisFile, suffix));

    const std::string commonPrefix = std::string(thisFile, 0, thisFile.size() - suffix.size());

    ASSERT_EQ("file.cpp", relativeSrcFilename(commonPrefix + "dir/file.cpp"));
    ASSERT_EQ("dir/file.cpp", relativeSrcFilename(commonPrefix + "toBeOmitted/dir/file.cpp"));
}
