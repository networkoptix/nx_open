#include "output_redirector.h"

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>

#if defined(_WIN32)
    #include <windows.h>
    #include <winbase.h>
    #include <shellapi.h>
    #include <atlstr.h>
    #include <codecvt>
    #pragma warning(disable: 4996) //< MSVC: freopen() is unsafe.
#elif defined(__linux__)
    #include <libgen.h>
    #include <memory.h>
#elif defined(__APPLE__)
    #include <libgen.h>
    #include <crt_externs.h>
#endif

#include "ini_config.h"

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

/*static*/ void OutputRedirector::redirectStdoutAndStderrIfNeeded(const char* overridingLogFilesDir/* = nullptr*/)
{
    const std::string logFilesDir = overridingLogFilesDir ? overridingLogFilesDir : nx::kit::IniConfig::iniFilesDir();

    const std::string processName = getProcessName();

    static const std::string kStdoutFilename = processName + "_stdout.log";
    static const std::string kStderrFilename = processName + "_stderr.log";

    if (fileExists(logFilesDir + kStdoutFilename))
        redirectOutput(stdout, "stdout", logFilesDir + kStdoutFilename);

    if (fileExists(logFilesDir + kStderrFilename))
        redirectOutput(stderr, "stderr", logFilesDir + kStderrFilename);
}

/*static*/ std::string OutputRedirector::getProcessName()
{
    #if defined(__linux__)
        std::ifstream inputStream("/proc/self/cmdline");
        std::string path;
        std::getline(inputStream, path);
        // Needed because basename() changes the string.
        char* const path_s = strdup(path.c_str());
        const std::string name = (strlen(path_s) == 0) ? "" : basename(path_s);
        free(path_s);
    #elif defined(__APPLE__)
        // Needed because basename() changes the string.
        char* const path_s = strdup((*_NSGetArgv())[0]);
        const std::string name = (strlen(path_s) == 0) ? "" : basename(path_s);
        free(path_s);
    #elif defined(_WIN32)
        int argc;
        LPWSTR* const argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (!argv)
            return "";
        const std::wstring wPath = argv[0];
        const size_t lastSeparatorPos = wPath.find_last_of(L"\\/");
        const std::wstring wNameWithExt =
            wPath.substr((lastSeparatorPos == std::wstring::npos) ? 0 : (lastSeparatorPos + 1));
        const std::wstring exeExt = L".exe";
        const std::wstring wName = (wNameWithExt.substr(wNameWithExt.size() - exeExt.size()) == exeExt)
            ? wNameWithExt.substr(0, wNameWithExt.size() - exeExt.size())
            : wNameWithExt;
        const std::string name = std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t>().to_bytes(wName);
        LocalFree(argv);
    #else
        #error "Not supported target OS"
    #endif

    return name;
}

#if !defined(NX_OUTPUT_REDIRECTOR_DISABLED)
    /** The redirection is performed by this static initialization. */
    static const OutputRedirector& redirect = OutputRedirector::getInstance();
#endif

static void redirectOutput(FILE* stream, const char* streamName, const std::string& filename)
{
    if (freopen(filename.c_str(), "w", stream))
        fprintf(stream, "%s is redirected to this file\n", streamName);
    // Ignore possible errors because it is not clear where to print the error message.
}

static bool fileExists(const std::string& filePath)
{
    return static_cast<bool>(std::ifstream(filePath.c_str()));
}

} // namespace kit
} // namespace nx
