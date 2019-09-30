#pragma once

#include <nx/kit/ini_config.h>

namespace nx::utils {

struct NX_UTILS_API Ini: nx::kit::IniConfig
{
    Ini(): IniConfig("nx_utils.ini") { reload(); }

    #if defined(_DEBUG)
        static constexpr const char* kDefaultMutexImplementation = "debug";
        static constexpr int kDefaultAssertCrash = 1;
        static constexpr int kDefaultAssertHeavyCondition = 1;
        static constexpr int kShowPasswordsInLogs = 1;
        static constexpr int kLogLevelReducerPassLimit = 100;
        static constexpr int kLogLevelReducerWindowSizeS = 20;
    #else
        static constexpr const char* kDefaultMutexImplementation = "qt";
        static constexpr int kDefaultAssertCrash = 0;
        static constexpr int kDefaultAssertHeavyCondition = 0;
        static constexpr int kShowPasswordsInLogs = 0;
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

    NX_INI_FLAG(kShowPasswordsInLogs, showPasswordsInLogs,
        "Show passwords in the log messages.");

    NX_INI_INT(kLogLevelReducerPassLimit, logLevelReducerPassLimit,
        "Replace error and warning logs with debugs after X same messages.");

    NX_INI_INT(kLogLevelReducerWindowSizeS, logLevelReducerWindowSizeS,
        "Replace error and warning logs with debugs within this time.");
};

NX_UTILS_API Ini& ini();

} // namespace nx::utils
