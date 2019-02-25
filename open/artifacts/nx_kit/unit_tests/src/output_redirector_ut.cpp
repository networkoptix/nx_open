#if defined(_WIN32)
    #include <stdio.h>
    #include <io.h>
    #define dup _dup
    #define dup2 _dup2
    #define fileno _fileno
#else
    #undef __STRICT_ANSI__ //< Enable fileno() in <stdio.h> on Cygwin/MinGW.
    #include <stdio.h>
    
    #include <unistd.h>
#endif
#include <fstream>
#include <string>
#include <cstring>

#include <nx/kit/test.h>
#include <nx/kit/output_redirector.h>

using namespace nx::kit;

class OutputRedirectorAdapter: public OutputRedirector
{
public:
    static void redirectStdoutAndStderrIfNeeded(const char *overridingLogFilesDir)
    {
        OutputRedirector::redirectStdoutAndStderrIfNeeded(overridingLogFilesDir);
    }

    static std::string getProcessName()
    {
        return OutputRedirector::getProcessName();
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

        OutputRedirectorAdapter::redirectStdoutAndStderrIfNeeded(testDir.c_str());

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
    const std::string processName = OutputRedirectorAdapter::getProcessName();
    const std::string stdoutFilePath = testDir + processName + std::string("_stdout.log");
    const std::string stderrFilePath = testDir + processName + std::string("_stderr.log");
};

/**
 * Redirection test should be performed before any output is made from the executable (by other
 * unit tests or the unit test framework), otherwise it may work incorrectly in Windows due to
 * some MSVC C++ Runtime issues (the output disappears at all).
 */
static OutputRedirectorTest outputRedirectorTest;

TEST(outputRedirector, check)
{
    //< Test output already performed during static initialization, so we need only to check its consequences.

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

    ASSERT_EQ("stdout is redirected to this file\nstdout test\ncout test\n", stdoutLogContent);
    ASSERT_EQ("stderr is redirected to this file\nstderr test\ncerr test\n", stderrLogContent);
}
