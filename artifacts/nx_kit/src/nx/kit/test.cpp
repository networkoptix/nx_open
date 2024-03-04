// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "test.h"

#include <chrono>
#include <ctime>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cstdlib>
#include <sstream>
#include <vector>
#include <fstream>
#include <memory>

#if defined(_WIN32)
    #include <direct.h> //< For mkdir().
#else
    #include <sys/stat.h> //< For mkdir().
#endif

namespace nx {
namespace kit {
namespace test {

bool verbose = true;

using nx::kit::utils::kPathSeparator;

namespace detail {

struct TestFailure: std::exception
{
    const char* const file;
    const int line;
    const int actualLine;
    const std::string message;

    TestFailure(const char* file, int line, int actualLine, std::string message):
        file(file), line(line), actualLine(actualLine), message(std::move(message))
    {
    }

    virtual const char* what() const noexcept override { return message.c_str(); }
};

void failEq(
    const std::string& expectedValue, const char* const expectedExpr,
    const std::string& actualValue, const char* const actualExpr,
    const char* const file, int line, int actualLine /*= -1*/)
{
    throw TestFailure(file, line, actualLine,
        "    Expected: [" + expectedValue + "] (" + expectedExpr + ")\n" +
        "    Actual:   [" + actualValue + "] (" + actualExpr + ")");
}

void assertStrEq(
    const UniversalString& expectedValue, const char* expectedExpr,
    const UniversalString& actualValue, const char* actualExpr,
    const char* file, int line, int actualLine)
{
    if (expectedValue.isNull)
    {
        throw TestFailure(file, line, actualLine,
            std::string("    INTERNAL ERROR: Expected string is null (") + expectedExpr + ")\n");
    }

    assertEq(expectedValue, expectedExpr, actualValue, actualExpr, file, line, actualLine);
}

void assertBool(
    bool expected, bool condition, const char* conditionStr,
    const char* file, int line, int actualLine /*= -1*/)
{
    if (expected != condition)
    {
        throw TestFailure(file, line, actualLine,
            "    Expected " + nx::kit::utils::toString(expected)
            + ", but is " + nx::kit::utils::toString(condition)
            + ": " + conditionStr);
    }
}

static std::vector<Test>& allTests()
{
    static std::vector<Test> allTests;
    return allTests;
}

/** Points to the currently running test, or nullptr if no test is running. */
static Test*& currentTest()
{
    static Test* value = nullptr;
    return value;
}

static std::string& suiteId()
{
    static std::string value;
    if (value.empty())
    {
        std::ostringstream s;
        s << std::hex << (uintptr_t) &allTests();
        value = s.str();
    }
    return value;
}

static void printSectionHeader(const char* formatStr, ...)
{
    static bool firstSection = true;
    if (firstSection)
        firstSection = false;
    else
        std::cerr << std::endl;

    std::cerr << "========================================================================"
        << std::endl;

    va_list args;
    va_start(args, formatStr);
    vfprintf(stderr, formatStr, args);
    va_end(args);

    std::cerr << std::endl;
}

static std::string testLogPrefix(const char* caption)
{
    if (currentTest() != nullptr)
    {
        // Inside the test there is no need to print test suite id or name, nor test case and name.
        return std::string(caption) + ": ";
    }

    return std::string(caption) + ": Suite [" + suiteId() + "]: ";
}

[[noreturn]] static void fatalError(const char* formatStr, ...)
{
    va_list args;
    va_start(args, formatStr);
    vfprintf(stderr, (testLogPrefix("FATAL ERROR") + formatStr + "\n").c_str(), args);
    va_end(args);

    exit(64);
}

static void printNote(const char* formatStr, ...)
{
    va_list args;
    va_start(args, formatStr);
    vfprintf(stderr, (testLogPrefix("NOTE") + formatStr + "\n").c_str(), args);
    va_end(args);
}

int regTest(const Test& test)
{
    allTests().push_back(test);

    if (verbose)
    {
        std::cerr << "Suite [" + suiteId() + "]: Added test #" << allTests().size() << ": "
            << test.testCaseDotName << std::endl;
    }

    return 0; //< Return value is not used.
}

struct ParsedCmdLineArgs
{
    bool showHelp = false;
    std::string explicitBaseTempDir;
    bool stopOnFirstFailure = false;
};

static const ParsedCmdLineArgs& parsedCmdLineArgs()
{
    static std::unique_ptr<ParsedCmdLineArgs> parsedArgs;

    if (parsedArgs)
        return *parsedArgs;

    parsedArgs.reset(new ParsedCmdLineArgs);

    const std::vector<std::string>& args = nx::kit::utils::getProcessCmdLineArgs();

    if (args.size() == 1)
        return *parsedArgs; //< Do nothing if no args were specified.

    const auto arg = [&args](int i) { return ((int) args.size() <= i) ? "" : args[i]; };

    if (args.size() == 2 && (arg(1) == "-h" || arg(1) == "--help"))
    {
        parsedArgs->showHelp = true;
        return *parsedArgs;
    }

    for (int i = /*skip argv[0]*/ 1; i < (int) args.size(); ++i)
    {
        // Stop processing args after "--" - let main() process them.
        if (arg(i) == "--")
            break;

        const std::string tmpOption = "--tmp";
        const std::string tmpOptionEq = "--tmp=";
        if (arg(i) == tmpOption)
        {
            parsedArgs->explicitBaseTempDir = arg(++i);
            if (parsedArgs->explicitBaseTempDir.empty())
                fatalError("Invalid command line args: no param for --tmp; run with --help.");
        }
        else if (arg(i).compare(0, tmpOptionEq.size(), tmpOptionEq) == 0) //< starts with
        {
            parsedArgs->explicitBaseTempDir = arg(i).substr(tmpOptionEq.size());
        }
        else if (arg(i) == "--stop-on-failure")
        {
            parsedArgs->stopOnFirstFailure = true;
        }
        else
        {
            fatalError("Unknown command line arg %s; run with --help.",
                nx::kit::utils::toString(arg(i)).c_str());
        }
    }

    return *parsedArgs;
}

/** NOTE: The trailing '\n' in a string (if any) is treated as an empty line. */
static std::vector<std::string> splitMultilineText(const std::string& text)
{
    std::vector<std::string> result;
    std::string lineText;
    std::istringstream stream(text);

    bool missingFinalNewline = true;
    while (std::getline(stream, lineText, '\n'))
    {
        result.push_back(lineText);
        missingFinalNewline = stream.eof();
    }
    if (!missingFinalNewline)
        result.push_back("");

    return result;
}

static void printMultilineText(
    const std::vector<std::string>& lines,
    const std::string& tag,
    const std::string substrToReplace,
    const std::string& substrReplacement)
{
    std::cerr << "\n" << tag << " (shown with line numbers, final '\\n' (if any) as empty line,\n"
        << (!substrToReplace.empty()
            ? ("substring " + nx::kit::utils::toString(substrToReplace) + "\n"
                + "replaced with " + nx::kit::utils::toString(substrReplacement) + ",\n")
            : "")
        << "non-printable chars as '?', trailing spaces as '#'):\n\n";

    if (lines.empty())
    {
        std::cerr << "  (" << tag << " is empty.)\n";
        return;
    }

    for (int lineIndex = 0; lineIndex < (int) lines.size(); ++lineIndex)
    {
        std::string printable = lines[lineIndex];

        // Replace non-printable chars with '?'.
        for (int i = 0; i < (int) printable.size(); ++i)
        {
            if (!nx::kit::utils::isAsciiPrintable(printable[i]))
                printable[i] = '?';
        }

        // Replace trailing spaces with '#'.
        for (int i = (int) printable.size() - 1; i >= 0 && printable[i] == ' '; --i)
            printable[i] = '#';

        // Print with line number as `cat -n` does: 6 digits (space-padded), then two spaces.
        std::cerr << nx::kit::utils::format("%6d  ", /* 1-based line number*/ lineIndex + 1)
            << printable << "\n";
    }
}

} using namespace detail;

void assertMultilineTextEquals(
    const char* file, int line, const std::string& testCaseTag,
    const std::string& expected, const std::string& actual,
    const std::string actualSubstrToReplace, const std::string& actualSubstrReplacement)
{
    std::string actualToCompare = actual;

    if (!actualSubstrToReplace.empty())
    {
        ASSERT_FALSE(actualSubstrReplacement.empty()); //< internal error
        nx::kit::utils::stringReplaceAll(
            &actualToCompare, actualSubstrToReplace, actualSubstrReplacement);
    }

    if (expected == actualToCompare)
        return;

    std::cerr << "\nFAILURE DETAILS for test case \"" << testCaseTag << "\":\n";

    const std::vector<std::string> expectedLines = splitMultilineText(expected);
    const std::vector<std::string> actualLines = splitMultilineText(actualToCompare);

    printMultilineText(actualLines, "Actual text", actualSubstrToReplace, actualSubstrReplacement);

    for (int i = 0; i < (int) actualLines.size(); ++i)
    {
        if (i >= (int) expectedLines.size())
        {
            std::cerr << "\nActual text is " << actualLines.size() - expectedLines.size()
                << " line(s) longer than expected (" << expectedLines.size() << " lines).\n";
            break;
        }

        if (expectedLines[i] != actualLines[i])
        {
            std::cerr << "\nExpected line " << i + 1 << ": "
                << nx::kit::utils::toString(expectedLines[i]) << "\n";
            std::cerr << "  Actual line " << i + 1 << ": "
                << nx::kit::utils::toString(actualLines[i]) << "\n";
        }
    }

    if (actualLines.size() < expectedLines.size())
    {
        std::cerr << "\nActual text is " << expectedLines.size() - actualLines.size()
            << " line(s) shorter than expected.\n";
    }

    throw TestFailure(file, line, /*actualLine*/ -1,
        "The text in test case \"" + testCaseTag + "\" is not as expected; see details above.");
}

//-------------------------------------------------------------------------------------------------
// Temp dir.

/** @return Empty string if the variable is empty, is longer than 1024 bytes, or does not exist. */
static std::string getEnvVar(const std::string& envVarName)
{
    #if defined(_WIN32)
        char envVarValue[1024]{0};
        size_t size;
        if (getenv_s(&size, envVarValue, sizeof(envVarValue), envVarName.c_str()) != 0 || envVarValue[0] == '\0')
            return "";
        return envVarValue;
    #else
        // On Linux, getenv() is thread-safe.
        const char* const envVarValue = getenv(envVarName.c_str());
        if (!envVarValue)
            return "";
        return envVarValue;
    #endif
}

/** @return System temp dir, ending with the path separator. */
static std::string systemTempDir()
{
    #if defined(ANDROID) || defined(__ANDROID__)
        return "/sdcard/";
    #elif defined(_WIN32)
        const std::string tempEnvVar = getEnvVar("TEMP");
        if (tempEnvVar.empty())
            return ".\\"; //< Use current dir if the system temp is not available.
        return tempEnvVar + kPathSeparator;
    #else // Assuming Linux or MacOS systems.
        const std::string tmpdirEnvVar = getEnvVar("TMPDIR");
        if (tmpdirEnvVar.empty())
            return "/tmp/";
        return tmpdirEnvVar + kPathSeparator;
    #endif
}

/** Create directory, issuing a fatal error on failure. */
static void createDir(const std::string& dir)
{
    #if defined(_WIN32)
        const int resultCode = _mkdir(dir.c_str());
    #else
        const int resultCode = mkdir(dir.c_str(), /*octal*/ 0777);
    #endif
    if (resultCode != 0)
        fatalError("Unable to create dir: %s", dir.c_str());
}

static std::string randAsString()
{
    static bool randomized = false;
    if (!randomized)
    {
        const auto seed = (unsigned int)
            std::chrono::steady_clock::now().time_since_epoch().count();
        srand(seed);
        randomized = true;
        if (verbose)
            printNote("Randomized with seed %u", seed);
    }

    std::ostringstream s;
    s << rand();
    return s.str();
}

static std::string baseTempDir()
{
    static std::string value;
    if (value.empty())
    {
        if (parsedCmdLineArgs().explicitBaseTempDir.empty())
        {
            value = systemTempDir() + "nx_kit_test_" + randAsString() + kPathSeparator;
            createDir(value);
        }
        else
        {
            value = parsedCmdLineArgs().explicitBaseTempDir;

            // Append a path separator if needed.
            if (value.back() != '/' && value.back() != '\\')
                value.push_back(kPathSeparator);
        }
    }
    return value;
}

static void printHelp(const std::string& argv0, const char* specificArgsHelp)
{
    const std::string usageExample = argv0 + " [<options>]" +
        (specificArgsHelp ? " [-- <specific-args>]" : "");

    const std::string specificArgsSection = specificArgsHelp
        ? (std::string("\n<specific-args>:\n\n") + specificArgsHelp)
        : "";

    // NOTE: stderr is used to avoid mixing with the output from static initialization.
    std::cerr <<
R"(
Usage:

  )" + usageExample + R"(

<options>:

  -h|--help
    Show usage help.

  --stop-on-failure
    Stop on first test failure.

  --tmp[=]<temp-dir>
    Use <temp-dir> for temp files instead of a random dir in the system temp dir.
)" + specificArgsSection;
}

//-------------------------------------------------------------------------------------------------

const char* tempDir()
{
    Test* const test = currentTest();
    if (!test)
        fatalError("tempDir() called outside of a test.");

    if (test->tempDir.empty())
    {
        test->tempDir = baseTempDir() + test->testCaseDotName + kPathSeparator;
        createDir(test->tempDir);

        if (verbose)
            printNote("Created temp dir: %s", test->tempDir.c_str());
    }

    return test->tempDir.c_str();
}

const char* staticTempDir()
{
    Test* const test = currentTest();
    if (test)
        fatalError("tempDir() called inside a TEST() body.");

    static std::string staticTempDir;
    if (!staticTempDir.empty())
        return staticTempDir.c_str();
    staticTempDir = baseTempDir() + "static" + kPathSeparator;
    createDir(staticTempDir);

    if (verbose)
        printNote("Created temp dir for static tests: %s", staticTempDir.c_str());

    return staticTempDir.c_str();
}

static bool runTest(Test& test, int testNumber)
{
    printSectionHeader("Test #%lu: %s", testNumber, test.testCaseDotName);
    std::cerr << std::endl;

    currentTest() = &test;
    bool success = false;
    try
    {
        test.testFunc();
        success = true;
    }
    catch (const TestFailure& e)
    {
        std::cerr << std::endl
            << "Test #" << testNumber << " FAILED at line " << e.line
            << (e.actualLine < 0 ? "" : nx::kit::utils::format(" (actual line %d)", e.actualLine))
            << ", file " << e.file
            << std::endl;
        std::cerr << std::endl << e.message << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << std::endl
            << "Test #" << testNumber << " FAILED with the exception:\n"
            "    " << e.what() << std::endl;
    }
    catch (...)
    {
        std::cerr << std::endl
            << "Test #" << testNumber << " FAILED with an unknown exception." << std::endl;
    }

    currentTest() = nullptr;
    return success;
}

int runAllTests(const char *testSuiteName, const char* specificArgsHelp)
{
    if (parsedCmdLineArgs().showHelp)
    {
        printHelp(nx::kit::utils::getProcessCmdLineArgs()[0], specificArgsHelp);
        exit(0);
    }

    const std::string fullSuiteName =
        std::string("suite ") + testSuiteName + " [" + suiteId() + "]";

    std::cerr << std::endl
        << "Running " << allTests().size() << " test(s) from " << fullSuiteName << std::endl;

    std::vector<int> failedTests;
    for (int i = 1; i <= (int) allTests().size(); ++i)
    {
        if (!runTest(allTests()[i - 1], i))
        {
            if (parsedCmdLineArgs().stopOnFirstFailure)
                exit(1);
            failedTests.push_back(i);
        }
    }

    if (failedTests.size() == allTests().size())
    {
        printSectionHeader("All %lu test(s) FAILED in %s. See messages above.",
            failedTests.size(), fullSuiteName.c_str());
        return (int) failedTests.size();
    }
    if (failedTests.size() > 1)
    {
        printSectionHeader("%lu of %lu tests FAILED in %s. See messages above.",
            failedTests.size(), allTests().size(), fullSuiteName.c_str());
        return (int) failedTests.size();
    }
    if (failedTests.size() == 1)
    {
        printSectionHeader("Test #%lu FAILED in %s. See the message above.",
            failedTests.front(), fullSuiteName.c_str());
        return 1;
    }
    printSectionHeader("SUCCESS: All %lu test(s) PASSED in %s.",
        allTests().size(), fullSuiteName.c_str());
    return 0;
}

void createFile(const std::string& filename, const std::string& content)
{
    std::ofstream s(filename);
    if (!s)
        fatalError("Unable to create temp file: %s", filename.c_str());

    if (!(s << content))
        fatalError("Unable to write to temp file: %s", filename.c_str());

    if (verbose)
        printNote("Created temp file: %s", filename.c_str());
}

} // namespace test
} // namespace kit
} // namespace nx
