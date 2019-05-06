#pragma once

#include <string>

#if !defined(NX_KIT_API)
    #define NX_KIT_API
#endif

namespace nx {
namespace kit {

/**
 * The mechanism allows to perform output (stdout, stderr) redirection to files.
 *
 * To activate redirection, the user must create special files in the directory
 * nx::kit::IniConfig::iniFilesDir(), named as follows:
 * `<executable-name-without-extension>_stdout.log`
 * and/or
 * `<executable-name-without-extension>_stderr.log`
 *
 * If such a file exists, the respective stream will be appended to the file contents.
 *
 * Redirection is performed during static initialization, thus, to ensure that it occurs before any
 * other output, this library should be the first in the list of linked libraries of the
 * executable. Note that in Windows due to some MSVC C++ Runtime issues, if any output occurs
 * before the redirection, the redirection may not work (the output disappears at all).
 *
 * If needed, this mechanism can be disabled defining a macro at compiling output_redirector.cpp:
 * -DNX_OUTPUT_REDIRECTOR_DISABLED.
 */
class NX_KIT_API OutputRedirector
{
public:
    OutputRedirector(const OutputRedirector&) = delete;

    void operator=(const OutputRedirector&) = delete;

    static const OutputRedirector& getInstance();

    /**
     * Call from anywhere in the code of the executable that links with this library.
     *
     * This is needed to guarantee that the static initializer in this library (that performs
     * the redirection) is called, because if no symbols from this library are used in the
     * executable, the linker can optimize away any static initialization in this library.
     */
     static void ensureOutputRedirection();

protected: //< Intended for unit tests.
    OutputRedirector();

    static void redirectStdoutAndStderrIfNeeded(const char* overridingLogFilesDir = nullptr);

    static std::string getProcessName();
};

} // namespace kit
} // namespace nx
