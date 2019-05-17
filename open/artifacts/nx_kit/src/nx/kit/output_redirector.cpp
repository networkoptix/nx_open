#include "output_redirector.h"

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>

#include "ini_config.h"
#include "utils.h"

#if defined(_WIN32)
    #pragma warning(disable: 4996) //< MSVC: freopen() is unsafe.
#endif

namespace nx {
namespace kit {

static void redirectOutput(FILE* stream, const char* streamName, const std::string& filename);

static bool fileExists(const std::string& filePath);

OutputRedirector::OutputRedirector()
{
    redirectStdoutAndStderrIfNeeded();
}

/*static*/ void OutputRedirector::ensureOutputRedirection()
{
    // Should be empty. The only purpose for it is to ensure that the linker will not optimize away
    // static initialization for this library.
}

/*static*/ const OutputRedirector& OutputRedirector::getInstance()
{
    static const OutputRedirector redirector;

    return redirector;
}

/*static*/ void OutputRedirector::redirectStdoutAndStderrIfNeeded(
    const char* overridingLogFilesDir /*= nullptr*/)
{
    const std::string logFilesDir =
        overridingLogFilesDir ? overridingLogFilesDir : nx::kit::IniConfig::iniFilesDir();

    const std::string processName = nx::kit::utils::getProcessName();

    static const std::string kStdoutFilename = processName + "_stdout.log";
    static const std::string kStderrFilename = processName + "_stderr.log";

    if (fileExists(logFilesDir + kStdoutFilename))
        redirectOutput(stdout, "stdout", logFilesDir + kStdoutFilename);

    if (fileExists(logFilesDir + kStderrFilename))
        redirectOutput(stderr, "stderr", logFilesDir + kStderrFilename);
}

#if !defined(NX_OUTPUT_REDIRECTOR_DISABLED)
    /** The redirection is performed by this static initialization. */
    const OutputRedirector& unused_OutputRedirector = OutputRedirector::getInstance();
#endif

static void redirectOutput(FILE* stream, const char* streamName, const std::string& filename)
{
    if (!freopen(filename.c_str(), "w", stream))
    {
        fprintf(stderr, "ERROR: Unable to perform redirection of %s to %s\n",
            streamName, filename.c_str());
        return;
    }

    fprintf(stream, "%s is redirected to this file\n", streamName);
}

static bool fileExists(const std::string& filePath)
{
    return static_cast<bool>(std::ifstream(filePath.c_str()));
}

} // namespace kit
} // namespace nx
