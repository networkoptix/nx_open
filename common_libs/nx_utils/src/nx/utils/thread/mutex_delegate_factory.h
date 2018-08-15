#pragma once

#include "mutex.h"

#include <nx/kit/ini_config.h>

namespace nx::utils {

namespace MutexImplementations
{
    enum Value
    {
        undefined = 0,
        qt = 1 << 1,
        std = 1 << 2,
        debug = 1 << 3,
        analyze = 1 << 4 | debug,
    };

    QString NX_UTILS_API toString(Value value);
    Value NX_UTILS_API parse(const QString& value);

    #if defined(_DEBUG)
        static const constexpr Value kDefault = debug;
    #else
        static const constexpr Value kDefault = qt;
    #endif
}

struct NX_UTILS_API Ini: nx::kit::IniConfig
{
    Ini(): IniConfig("nx_utils.ini") { reload(); }

    #if defined(_DEBUG)
        static constexpr const char* kDefaultMutexImplementation = "debug";
    #else
        static constexpr const char* kDefaultMutexImplementation = "qt";
    #endif

    NX_INI_STRING(kDefaultMutexImplementation, mutexImplementation,
        "Mutex implementation: qt, std, debug or analyze.");
};

const Ini& ini();
MutexImplementations::Value NX_UTILS_API mutexImplementation();

std::unique_ptr<MutexDelegate> NX_UTILS_API makeMutexDelegate(Mutex::RecursionMode mode);
std::unique_ptr<ReadWriteLockDelegate> NX_UTILS_API makeReadWriteLockDelegate(ReadWriteLock::RecursionMode mode);
std::unique_ptr<WaitConditionDelegate> NX_UTILS_API makeWaitConditionDelegate();

} // namespace nx::utils
