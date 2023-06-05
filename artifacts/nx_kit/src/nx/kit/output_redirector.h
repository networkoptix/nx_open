// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
 *
 * ATTENTION: If all of the following conditions are met:
 *   1. The shared library is built on Windows as "Debug" with MSVC.
 *   2. The library is loaded dynamically in runtime.
 *   3. The main executable runs as a Windows service.
 * Then the redirected output doesn't go to the file and just disappears. This behavior is due to
 * some unknown MSVC issue.
 */
class NX_KIT_API OutputRedirector
{
public:
    OutputRedirector(const OutputRedirector&) = delete;

    void operator=(const OutputRedirector&) = delete;

    static const OutputRedirector& getInstance();

    bool isStdoutRedirected() const { return m_isStdoutRedirected; }
    bool isStderrRedirected() const { return m_isStderrRedirected; }

    /**
     * Call from anywhere in the code of the executable that links with this library.
     *
     * This is needed to guarantee that the static initializer in this library (that performs
     * the redirection) is called, because if no symbols from this library are used in the
     * executable, the linker can optimize away any static initialization in this library.
     *
     * ATTENTION: This call should be done exactly once to prevent unexpected behavior in the case
     * when more than one copy of the library is linked to the executable. This situation can
     * occur, for example, when one copy of nx::kit library is linked to the executable itself, and
     * another one is linked to the shared library. If ensureOutputRedirection() is called once
     * (in the executable) this works perfectly well, because the linker leaves static
     * initialization in the executable and optimizes it away from the shared library. Otherwise
     * (ensureOutputRedirection() is called both in the executable and in the shared library) it
     * can result in multiple initialization of the Redirector, and some output can be lost.
     */
     static void ensureOutputRedirection();

protected: //< Intended for unit tests.
    OutputRedirector(const char* overridingLogFilesDir = nullptr);

private:
    bool m_isStdoutRedirected = false;
    bool m_isStderrRedirected = false;
};

} // namespace kit
} // namespace nx
