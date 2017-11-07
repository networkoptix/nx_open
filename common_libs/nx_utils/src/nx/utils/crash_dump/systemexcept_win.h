#pragma once

#include <string>
#include <windows.h>

//!Used to dump unhandled structured exception call stack to file ({application_name}_{pid}.except near application binary)
/*!
    \note This does not work if structured exception handling is not used and application is being debugged
        (debugger handles unhandled exceptions by itself)
    \note This handling works on per-thread basis. \a win32_exception::translate has to be called in thread to enable unhandled structured exception handling
    \note QnLongRunnable instance calls \a win32_exception::translate after thread creation
    \todo some refactoring is required
*/
class NX_UTILS_API win32_exception
{
public:
    //!Registers handler to intercept system exceptions (e.g., Access violation)
    static void installGlobalUnhandledExceptionHandler();
    static void installThreadSpecificUnhandledExceptionHandler();
    //!
    /*!
        \param isFull If \a true then in case of process crash all process memory dumped.
            If \a false, only call stack for each thread is dumped.
            By default, \a false
    */
    static void setCreateFullCrashDump( bool isFull );

    //!How to find created dumps
    static std::string getCrashDirectory();
    static std::string getCrashPattern();
};
