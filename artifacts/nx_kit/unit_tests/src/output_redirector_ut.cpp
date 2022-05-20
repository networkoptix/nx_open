// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#if defined(_WIN32)
    #include <stdio.h>
    #include <io.h>
    #define dup _dup
    #define dup2 _dup2
    #define fileno _fileno
#else
    #if defined(__STRICT_ANSI__)
        #undef __STRICT_ANSI__ //< Enable fileno() in <stdio.h> on Cygwin/MinGW.
        #include <stdio.h>
        #define __STRICT_ANSI__
    #else
        #include <stdio.h>
    #endif

    #include <unistd.h>
#endif

#include <fstream>
#include <string>
#include <cstring>
#include <memory>

#include <nx/kit/utils.h>
#include <nx/kit/test.h>
#include <nx/kit/output_redirector.h>

using namespace nx::kit;

class OutputRedirectorAdapter: public OutputRedirector
{
public:
    OutputRedirectorAdapter(const char* overridingLogFilesDir):
        OutputRedirector(overridingLogFilesDir)
    {
    }
};

class OutputRedirectorTest
{
public:
    OutputRedirectorTest()
    {
        nx::kit::test::verbose = false;

        nx::kit::test::createFile(stdoutFilePath.c_str(), "");
        nx::kit::test::createFile(stderrFilePath.c_str(), "");

        int stdoutFileno = fileno(stdout);
        int stderrFileno = fileno(stderr);

        int stdoutFilenoBak = (stdoutFileno == -1) ? -1 : dup(stdoutFileno);
        int stderrFilenoBak = (stderrFileno == -1) ? -1 : dup(stderrFileno);

        outputRedirectorAdapter.reset(new OutputRedirectorAdapter(testDir.c_str()));

        nx::kit::test::verbose = true;

        fprintf(stderr, "stderr test\n");
        std::cerr << "cerr test" << std::endl;

        fprintf(stdout, "stdout test\n");
        std::cout << "cout test" << std::endl;

        if (stdoutFilenoBak != -1)
            dup2(stdoutFilenoBak, stdoutFileno);

        if (stderrFilenoBak != -1)
            dup2(stderrFilenoBak, stderrFileno);
    }

    const std::string testDir = nx::kit::test::staticTempDir();
    const std::string processName = nx::kit::utils::getProcessName();
    const std::string stdoutFilePath = testDir + processName + std::string("_stdout.log");
    const std::string stderrFilePath = testDir + processName + std::string("_stderr.log");
    
    std::unique_ptr<const OutputRedirectorAdapter> outputRedirectorAdapter;
};

/**
 * Redirection test should be performed before any output is made from the executable (by other
 * unit tests or the unit test framework), otherwise it may work incorrectly in Windows due to
 * some MSVC C++ Runtime issues (the output disappears at all).
 */
static OutputRedirectorTest outputRedirectorTest;

TEST(outputRedirector, check)
{
    ASSERT_TRUE(outputRedirectorTest.outputRedirectorAdapter->isStdoutRedirected());
    ASSERT_TRUE(outputRedirectorTest.outputRedirectorAdapter->isStderrRedirected());

    // Test output already performed during the static initialization in the constructor of
    // OutputRedirectorTest, so we need only to check its consequences.
    
    std::ifstream stdoutLogStream(outputRedirectorTest.stdoutFilePath);
    std::ifstream stderrLogStream(outputRedirectorTest.stderrFilePath);

    ASSERT_TRUE(stdoutLogStream.is_open());
    ASSERT_TRUE(stderrLogStream.is_open());

    std::string line;
    std::string stdoutLogContent;
    std::string stderrLogContent;

    while (std::getline(stdoutLogStream, line))
        stdoutLogContent += line + "\n";

    while (std::getline(stderrLogStream, line))
        stderrLogContent += line + "\n";

    stdoutLogStream.close();
    stderrLogStream.close();

    std::ostringstream stdoutLogExpectedContent;
    stdoutLogExpectedContent << "stdout of "
    << nx::kit::utils::toString(outputRedirectorTest.processName) << " is "
        << "redirected to this file ("
        << nx::kit::utils::toString(outputRedirectorTest.stdoutFilePath) << ")\n"
        << "stdout test\ncout test\n";

    std::ostringstream stderrLogExpectedContent;
    stderrLogExpectedContent << "stderr of "
        << nx::kit::utils::toString(outputRedirectorTest.processName) << " is "
        << "redirected to this file ("
        << nx::kit::utils::toString(outputRedirectorTest.stderrFilePath) << ")\n"
        << "stderr test\ncerr test\n";

    ASSERT_STREQ(stdoutLogExpectedContent.str(), stdoutLogContent);
    ASSERT_STREQ(stderrLogExpectedContent.str(), stderrLogContent);
}
