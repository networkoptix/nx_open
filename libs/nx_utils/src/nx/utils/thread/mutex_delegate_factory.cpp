#include "mutex_delegate_factory.h"

#include <nx/utils/log/log.h>
#include <nx/utils/nx_utils_ini.h>
#include <nx/utils/unused.h>

#include "mutex_delegates_debug.h"
#include "mutex_delegates_std.h"
#include "mutex_delegates_qt.h"

namespace nx::utils {

static constexpr auto kUndefinedName("undefined");
static constexpr auto kQtName("qt");
static constexpr auto kStdName("std");
static constexpr auto kDebugName("debug");
static constexpr auto kAnalyzeName("analyze");

QString MutexImplementations::toString(Value value)
{
    switch(value)
    {
        case MutexImplementations::undefined: return kUndefinedName;
        case MutexImplementations::qt: return kQtName;
        case MutexImplementations::std: return kStdName;
        case MutexImplementations::debug: return kDebugName;
        case MutexImplementations::analyze: return kAnalyzeName;
    }

    NX_ASSERT(false);
    return lm("undefined(%1)").arg(static_cast<int>(value));
}

MutexImplementations::Value MutexImplementations::parse(const QString& value)
{
    if (value == kQtName)
        return MutexImplementations::qt;

    if (value == kStdName)
        return MutexImplementations::std;

    if (value == kDebugName)
        return MutexImplementations::debug;

    if (value == kAnalyzeName)
        return MutexImplementations::analyze;

    return MutexImplementations::undefined;
}

MutexImplementations::Value mutexImplementation()
{
    static const auto value =
        []()
        {
            const auto value = ini().mutexImplementation;
            const auto parsed = MutexImplementations::parse(value);
            if (parsed != MutexImplementations::undefined)
                return parsed;

            NX_ASSERT(false, lm("Unknown mutex implementaiton in ini: %1").args(value));
            return MutexImplementations::parse(Ini::kDefaultMutexImplementation);
        }();

    return value;
}

std::unique_ptr<MutexDelegate> makeMutexDelegate(Mutex::RecursionMode mode)
{
    static const auto impl = mutexImplementation();
    if (impl & MutexImplementations::qt)
        return std::make_unique<MutexQtDelegate>(mode);

    #if defined(NX_UTILS_MUTEX_DELEGATES_STD)
        if (impl & MutexImplementations::std)
            return std::make_unique<MutexStdDelegate>(mode);
    #endif

    if (impl & MutexImplementations::debug)
        return std::make_unique<MutexDebugDelegate>(mode, impl == MutexImplementations::analyze);

    NX_ASSERT(false, lm("Unknown mutex implementation: %1").arg(impl));
    return std::make_unique<MutexQtDelegate>(mode);
}

std::unique_ptr<ReadWriteLockDelegate> makeReadWriteLockDelegate(ReadWriteLock::RecursionMode mode)
{
    static const auto impl = mutexImplementation();
    if (impl & MutexImplementations::qt)
        return std::make_unique<ReadWriteLockQtDelegate>(mode);

    #if defined(NX_UTILS_MUTEX_DELEGATES_STD)
        if (impl & MutexImplementations::std)
            return std::make_unique<ReadWriteLockStdDelegate>(mode);
    #endif

    if (impl & MutexImplementations::debug)
        return std::make_unique<ReadWriteLockDebugDelegate>(mode, impl == MutexImplementations::analyze);

    NX_ASSERT(false, lm("Unknown mutex implementation: %1").arg(impl));
    return std::make_unique<ReadWriteLockQtDelegate>(mode);
}

std::unique_ptr<WaitConditionDelegate> makeWaitConditionDelegate()
{
    static const auto impl = mutexImplementation();
    if (impl & MutexImplementations::qt)
        return std::make_unique<WaitConditionQtDelegate>();

    #if defined(NX_UTILS_MUTEX_DELEGATES_STD)
        if (impl & MutexImplementations::std)
            return std::make_unique<WaitConditionStdDelegate>();
    #endif

    if (impl & MutexImplementations::debug)
        return std::make_unique<WaitConditionDebugDelegate>();

    NX_ASSERT(false, lm("Unknown mutex implementation: %1").arg(impl));
    return std::make_unique<WaitConditionQtDelegate>();
}

} // namespace nx::utils

