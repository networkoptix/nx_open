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

#if defined(_WIN32)
    #include <direct.h> //< For mkdir().
#else
    #include <sys/stat.h> //< For mkdir().
#endif

namespace nx {
namespace kit {
namespace test {

bool verbose = true;

namespace detail {

struct TestFailure: std::exception
{
    const char* const file;
    const int line;
    const std::string message;

    TestFailure(const char* file, int line, std::string message):
        file(file), line(line), message(std::move(message))
    {
    }

    virtual const char* what() const noexcept override { return message.c_str(); }
};

void failEq(
    const char* expectedValue, const char* expectedExpr,
    const char* actualValue, const char* actualExpr,
    const char* const file, int line)
{
    throw TestFailure(file, line,
        std::string("    Expected: [") + expectedValue + "] (" + expectedExpr + ")\n" +
        std::string("    Actual:   [") + actualValue + "] (" + actualExpr + ")");
}

void assertStrEq(
    const char* expectedValue, const char* expectedExpr,
    const char* actualValue, const char* actualExpr,
    const char* file, int line)
{
    using nx::kit::utils::toString;

    if (expectedValue == nullptr)
    {
        throw TestFailure(file, line,
            std::string("    INTERNAL ERROR: Expected string is null: [")
                + toString(expectedExpr) + "]\n");
    }

    if (actualValue == nullptr || strcmp(expectedValue, actualValue) != 0)
    {
        throw TestFailure(file, line, nx::kit::utils::format(
            "    Expected: [%s] (%s)\n"
            "    Actual:   [%s] (%s)",
            toString(expectedValue).c_str(), expectedExpr,
            toString(actualValue).c_str(), actualExpr));
    }
}

static std::string boolToString(bool value)
{
    return value ? "true" : "false";
}

void assertBool(
    bool expected, bool condition, const char* conditionStr, const char* file, int line)
{
    if (expected != condition)
    {
        throw TestFailure(file, line,
            "    Expected " + boolToString(expected) + ", but is " + boolToString(condition)
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
};

static const ParsedCmdLineArgs& parsedCmdLineArgs()
{
    static std::unique_ptr<ParsedCmdLineArgs> parsedArgs;

    if (parsedArgs)
        return *parsedArgs;

    parsedArgs.reset(new ParsedCmdLineArgs);

    const auto& args = nx::kit::utils::getProcessCmdLineArgs();

    if (args.size() == 1)
        return *parsedArgs; //< Do nothing if no args were specified.

    const auto arg = [&args](int i) { return ((int) args.size() <= i) ? "" : args[i]; };

    if (args.size() == 2 && (arg(1) == "-h" || arg(1) == "--help"))
    {
        parsedArgs->showHelp = true;
        return *parsedArgs;
    }

    const std::string tmpOption = "--tmp";
    const std::string tmpOptionEq = "--tmp=";
    if (arg(1) == tmpOption)
    {
        if (args.size() < 3)
            fatalError("Invalid command line args: no param for --tmp; run with \"--help\".");
        parsedArgs->explicitBaseTempDir = arg(2);
    }
    else if (arg(1).compare(0, tmpOptionEq.size(), tmpOptionEq) == 0) //< Starts with tmpOptionEq.
    {
        parsedArgs->explicitBaseTempDir = arg(1).substr(tmpOptionEq.size());
    }
    else
    {
        fatalError("Unknown command line arg [%s]; run with \"--help\".", arg(1).c_str());
    }

    return *parsedArgs;
}

} using namespace detail;

//-------------------------------------------------------------------------------------------------
// Temp dir.

#if defined(_WIN32)
    static constexpr char kPathSeparator[] = "\\";
#else
    static constexpr char kPathSeparator[] = "/";
#endif

/** @return System temp dir, ending with the path separator. */
static std::string systemTempDir()
{
    #if defined(ANDROID) || defined(__ANDROID__)
        return "/sdcard/";
    #elif defined(_WIN32)
        char env[1024]{0};
        size_t size;
        if (getenv_s(&size, env, sizeof(env), "TEMP") != 0 || env[0] == '\0')
            return ".\\"; //< Use current dir if the system temp is not available.
        return std::string(env) + kPathSeparator;
    #else // Assuming Linux or MacOS systems.
        return "/tmp/";
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
        fatalError("Unable to create dir: [%s]", dir.c_str());
}

static std::string randAsString()
{
    static bool randomized = false;
    if (!randomized)
    {
        auto seed = (unsigned int) std::chrono::steady_clock::now().time_since_epoch().count();
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
            const char lastChar = value[value.size() - 1];
            if (lastChar != '/' && lastChar != '\\')
                value.append(kPathSeparator);
        }
    }
    return value;
}

static void printHelp(const std::string& argv0)
{
    std::cerr << //< stderr is used to avoid mixing with the output from static initialization.
        "\n" <<
        "Usage:\n" <<
        "\n" <<
        "  " << argv0 << " [options]\n" <<
        "\n" <<
        "Options:\n" <<
        "\n" <<
        "  -h|--help\n" <<
        "    Show usage help.\n" <<
        "\n" <<
        "  --tmp=<temp-dir>\n" <<
        "    Use <temp-dir> for temp files instead of a random dir in the system temp dir.\n" <<
        "";
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
            printNote("Created temp dir: [%s]", test->tempDir.c_str());
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
        printNote("Created temp dir for static tests: [%s]", staticTempDir.c_str());

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
            << "Test #" << testNumber << " FAILED at line " << e.line << ", file " << e.file
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

int runAllTests(const char *testSuiteName)
{
    if (parsedCmdLineArgs().showHelp)
    {
        printHelp(nx::kit::utils::getProcessCmdLineArgs()[0]);
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
            failedTests.push_back(i);
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

void createFile(const char* filename, const char* content)
{
    std::ofstream s(filename);
    if (!s)
        fatalError("Unable to create temp file: [%s]", filename);

    if (!(s << content))
        fatalError("Unable to write to temp file: [%s]", filename);

    if (verbose)
        printNote("Created temp file: [%s]", filename);
}

} // namespace test
} // namespace kit
} // namespace nx
