// Copyright 2017 Network Optix, Inc. Licensed under GNU Lesser General Public License version 3.
#include "test.h"

#include <ctime>
#include <cstdio>
#include <cstdarg>
#include <vector>
#include <cstring>

namespace nx {
namespace kit {
namespace test {

namespace detail {

struct TestFailure: std::exception
{
    const char* const file;
    const int line;
    const std::string message;

    TestFailure(const char* file, int line, const std::string& message):
        file(file), line(line), message(message)
    {
    }

    virtual const char* what() const noexcept { return message.c_str(); }
};

void failEq(
    const std::string& expectedValue, const char* expectedExpr,
    const std::string& actualValue, const char* actualExpr,
    const char* const file, int line)
{
    throw TestFailure(file, line,
        "    Expected: [" + expectedValue + "] (" + expectedExpr + ")\n" +
        "    Actual:   [" + actualValue + "] (" + actualExpr + ")");
}

static void printSection(const std::string formatStr, ...)
{
    static bool firstSection = true;
    if (firstSection)
        firstSection = false;
    else
        std::cerr << std::endl;

    std::cerr << "==================================================================" << std::endl;

    va_list args;
    va_start(args, formatStr);

    vfprintf(stderr, formatStr.c_str(), args);

    va_end(args);
    std::cerr << std::endl;
}

static std::vector<Test>& allTests()
{
    static std::vector<Test> allTests;
    return allTests;
}

void assertBool(
    bool expected, bool cond, const char* const condStr, const char* const file, int line)
{
    if (expected != cond)
    {
        throw TestFailure(file, line,
            std::string("    ") + ((expected == true) ? "False" : "True") + ": " + condStr);
    }
}

int regTest(const Test& test)
{
    allTests().push_back(test);

    std::cerr << "Test suite [" << std::hex << (uintptr_t) &allTests() << std::dec
        << "]: Adding test #" << allTests().size() << ": "
        << test.testCase << "." << test.testName << std::endl;

    return 0; //< Return value is not used.
}

} using namespace detail;

int runAllTests()
{
    std::cerr << std::endl << "Running " << allTests().size() << " test(s) from test suite ["
        << std::hex << (uintptr_t) &allTests() << "]" << std::dec << std::endl;

    std::vector<size_t> failedTests;
    for (size_t i = 1; i <= allTests().size(); ++i)
    {
        const auto& test = allTests().at(i - 1);

        printSection("Test #%lu: " + test.testCase + "." + test.testName, i);
        std::cerr << std::endl;

        try
        {
            test.testFunc();
            //std::cerr << std::endl << "PASSED." << std::endl;
        }
        catch (const TestFailure& e)
        {
            std::cerr << std::endl
                << "Test #" << i << " FAILED at line " << e.line << ", file " << e.file
                << std::endl;
            std::cerr << std::endl << e.message << std::endl;
            failedTests.push_back(i);
        }
        catch (const std::exception& e)
        {
            std::cerr << std::endl
                << "Test #" << i << " FAILED with the exception:\n"
                "    " << e.what() << std::endl;
            failedTests.push_back(i);
        }
        catch (...)
        {
            std::cerr << std::endl
                << "Test #" << i << " FAILED with an unknown exception." << std::endl;
            failedTests.push_back(i);
        }
    }

    if (failedTests.size() == allTests().size())
    {
        printSection("All %lu test(s) FAILED. See messages above.", failedTests.size());
        return (int) failedTests.size();
    }
    if (failedTests.size() > 1)
    {
        printSection("Some tests FAILED: %lu of %lu. See messages above.",
            failedTests.size(), allTests().size());
        return (int) failedTests.size();
    }
    if (failedTests.size() == 1)
    {
        printSection("Test #%lu FAILED. See the message above.", failedTests.front());
        return 1;
    }
    printSection("SUCCESS: All %lu test(s) PASSED.", allTests().size());
    return 0;
}

//-------------------------------------------------------------------------------------------------
// Temp files

std::string tempDir()
{
    static std::string tempDir;

    if (tempDir.empty())
    {
        char tempDirStr[L_tmpnam] = "";
        ASSERT_TRUE(std::tmpnam(tempDirStr) != nullptr);

        // Extract dir name from the file path - truncate after the last path separator.
        for (int i = (int) strlen(tempDirStr) - 1; i >= 0; --i)
        {
            if (tempDirStr[i] == '/' || tempDirStr[i] == '\\')
            {
                tempDirStr[i + 1] = '\0';
                break;
            }
        }

        tempDir = tempDirStr;
        std::cerr << "NOTE: Using temp path for creating temp files: " << tempDir << std::endl;
    }

    return tempDir;
}

TempFile::TempFile(const std::string& prefix, const std::string& suffix)
{
    static bool randomized = false;
    if (!randomized)
    {
        srand((unsigned int) time(nullptr));
        randomized = true;
    }

    char randStr[20];
    snprintf(randStr, sizeof(randStr), "%d", rand());
    m_filename = tempDir() + prefix + randStr + suffix;
}

TempFile::~TempFile()
{
    if (keepFiles())
    {
        std::cerr << "ATTENTION: Temp file not deleted because "
            << "NX_KIT_TEST_KEEP_TEMP_FILES is defined: " << m_filename << std::endl;
        return;
    }

    if (std::uncaught_exception()) //< A test failed - keep the file.
    {
        std::cerr << "ATTENTION: Temp file not deleted because an exception was thrown: "
            << m_filename << std::endl;
        return;
    }

    std::remove(m_filename.c_str());
}

/*static*/ bool& TempFile::keepFiles()
{
    static bool value = false;
    return value;
}

} // namespace test
} // namespace kit
} // namespace nx
