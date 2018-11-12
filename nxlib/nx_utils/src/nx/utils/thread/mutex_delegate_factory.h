#pragma once

#include "mutex.h"

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

MutexImplementations::Value NX_UTILS_API mutexImplementation();

std::unique_ptr<MutexDelegate> NX_UTILS_API makeMutexDelegate(Mutex::RecursionMode mode);
std::unique_ptr<ReadWriteLockDelegate> NX_UTILS_API makeReadWriteLockDelegate(ReadWriteLock::RecursionMode mode);
std::unique_ptr<WaitConditionDelegate> NX_UTILS_API makeWaitConditionDelegate();

} // namespace nx::utils
