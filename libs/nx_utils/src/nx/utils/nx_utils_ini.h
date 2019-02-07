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
    #else
        static constexpr const char* kDefaultMutexImplementation = "qt";
        static constexpr int kDefaultAssertCrash = 0;
        static constexpr int kDefaultAssertHeavyCondition = 0;
        static constexpr int kShowPasswordsInLogs = 0;
    #endif

    NX_INI_STRING(kDefaultMutexImplementation, mutexImplementation,
        "Mutex implementation: qt, std, debug or analyze.");

    NX_INI_FLAG(kDefaultAssertCrash, assertCrash,
        "Crash application on assertion failure.");

    NX_INI_FLAG(kDefaultAssertCrash, assertHeavyCondition,
        "Enable assertions for heavy conditions.");

    NX_INI_FLAG(kShowPasswordsInLogs, showPasswordsInLogs,
        "Show passwords in the log messages.");
};

NX_UTILS_API Ini& ini();

} // namespace nx::utils
