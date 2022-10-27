// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/kit/ini_config.h>

namespace nx::utils {

struct NX_UTILS_API Ini: nx::kit::IniConfig
{
    Ini(): IniConfig("nx_utils.ini") { reload(); }

    static constexpr int kShowPasswordsInLogs = 0;
    #if defined(_DEBUG)
        static constexpr int kDefaultPrintStackTraceOnAssert = 1;
        static constexpr const char* kDefaultMutexImplementation = "debug";
        static constexpr int kDefaultAssertCrash = 1;
        static constexpr int kDefaultAssertHeavyCondition = 1;
        static constexpr int kLogLevelReducerPassLimit = 100;
        static constexpr int kLogLevelReducerWindowSizeS = 20;
    #else
        static constexpr int kDefaultPrintStackTraceOnAssert = 0;
        static constexpr const char* kDefaultMutexImplementation = "qt";
        static constexpr int kDefaultAssertCrash = 0;
        static constexpr int kDefaultAssertHeavyCondition = 0;
        static constexpr int kLogLevelReducerPassLimit = 100;
        static constexpr int kLogLevelReducerWindowSizeS = 600;
    #endif

    NX_INI_STRING(kDefaultMutexImplementation, mutexImplementation,
        "Mutex implementation:\n"
        "qt - fastest, default for release;\n"
        "std - average speed, useful for valgrind;\n"
        "debug - average speed, provides more info for debugger, default for debug;\n"
        "analyze - very slow, analyses mutexes for deadlocks.");

    NX_INI_FLAG(kDefaultAssertCrash, assertCrash,
        "Crash application on assertion failure.");

    NX_INI_FLAG(kDefaultAssertHeavyCondition, assertHeavyCondition,
        "Enable assertions for heavy conditions.");

    NX_INI_FLAG(kDefaultPrintStackTraceOnAssert, printStackTraceOnAssert,
        "Print stack trace leading to failed assertion.");

    #if defined(_DEBUG)
        NX_INI_FLAG(kShowPasswordsInLogs, showPasswordsInLogs,
            "Show passwords in the log messages.");
    #else
        const bool showPasswordsInLogs = 0;
    #endif

    NX_INI_INT(kLogLevelReducerPassLimit, logLevelReducerPassLimit,
        "Replace error and warning logs with debugs after X same messages.");

    NX_INI_INT(kLogLevelReducerWindowSizeS, logLevelReducerWindowSizeS,
        "Replace error and warning logs with debugs within this time.");

    NX_INI_FLOAT(1.0, valueHistoryAgeDelimiter,
        "Reduces max age of all ValueHistory storage and requests.");

    NX_INI_FLOAT(1.0, loadFactor,
        "Load factor for unit tests, in range (0..1]. The smaller this value is, the less amount\n"
        "of testing is performed by certain unit tests, which leads to faster test execution and\n"
        "less system load. For details, see the usages in the C++ code.");

    NX_INI_STRING("dd.MM.yyyy HH:mm:ss.zzz UTC", debugTimeRepresentation,
        "Representation for timestamp in developer logs (see `timestampToDebugString` function)");
};

NX_UTILS_API Ini& ini();

} // namespace nx::utils
