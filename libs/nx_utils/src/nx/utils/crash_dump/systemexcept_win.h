// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

//!Used to dump unhandled structured exception call stack to file ({application_name}_{pid}.except near application binary)
/*!
    NOTE: This does not work if structured exception handling is not used and application is being debugged
        (debugger handles unhandled exceptions by itself)
    NOTE: This handling works on per-thread basis. win32_exception::translate has to be called in thread to enable unhandled structured exception handling
    NOTE: QnLongRunnable instance calls win32_exception::translate after thread creation
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
        \param isFull If true then in case of process crash all process memory dumped.
            If false, only call stack for each thread is dumped.
            By default, false
    */
    static void setCreateFullCrashDump( bool isFull );

    //!How to find created dumps
    static std::string getCrashDirectory();
    static std::string getCrashPattern();
};
