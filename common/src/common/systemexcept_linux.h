#ifdef __linux__

#ifndef SYSTEMEXCEPT_LINUX_H
#define SYSTEMEXCEPT_LINUX_H

#include <string>

//! Used to dump unhandled structured exception call stack to file ~/{application_name}_{pid}.crash
/*!
 *  \note after stack collection original signal handler is called, so core can still be dumped
 */
class linux_exception
{
public:
    //! Registers handler to catch system signals (e.g., SIGSEGV)
    static void installCrashSignalHandler();

    //! Accounts threads for backtrace in case of system signal
    static void installQuitThreadBacktracer();
    static void uninstallQuitThreadBacktracer();

    //! Use to disable signals handling (to see core dump unaffected)
    static void setSignalHandlingDisabled(bool isDisabled);

    //! How to find created dumps
    static std::string getCrashDirectory();
    static std::string getCrashPattern();
};

#endif // SYSTEMEXCEPT_LINUX_H

#endif // __linux__
