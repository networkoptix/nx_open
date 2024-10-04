// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "mutex_delegate_factory.h"

#include <nx/utils/log/log.h>
#include <nx/utils/nx_utils_ini.h>

#include "mutex_delegates_debug.h"
#include "mutex_delegates_std.h"
#include "mutex_delegates_qt.h"

namespace nx {

static constexpr auto kUndefinedName("undefined");
static constexpr auto kQtName("qt");
static constexpr auto kStdName("std");
static constexpr auto kDebugName("debug");
static constexpr auto kAnalyzeName("analyze");

QString MutexImplementations::toString(Value value)
{
    switch (value)
    {
        case MutexImplementations::undefined: return kUndefinedName;
        case MutexImplementations::qt: return kQtName;
        case MutexImplementations::std: return kStdName;
        case MutexImplementations::debug: return kDebugName;
        case MutexImplementations::analyze: return kAnalyzeName;
    }

    NX_ASSERT(false);
    return nx::format("undefined(%1)", static_cast<int>(value));
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
            const auto impl = utils::ini().mutexImplementation;
            const auto parsed = MutexImplementations::parse(impl);
            if (parsed != MutexImplementations::undefined)
                return parsed;

            NX_ASSERT(false, "Unknown mutex implementation in ini: %1", impl);
            return MutexImplementations::parse(utils::Ini::kDefaultMutexImplementation);
        }();

    return value;
}

std::unique_ptr<MutexDelegate> makeMutexDelegate(Mutex::RecursionMode mode)
{
    static const auto impl = mutexImplementation();
    if (impl & MutexImplementations::qt)
        return std::make_unique<MutexQtDelegate>(mode);

    if (impl & MutexImplementations::std)
        return std::make_unique<MutexStdDelegate>(mode);

    if (impl & MutexImplementations::debug)
        return std::make_unique<MutexDebugDelegate>(mode, impl == MutexImplementations::analyze);

    NX_ASSERT(false, "Unknown mutex implementation: %1", impl);
    return std::make_unique<MutexQtDelegate>(mode);
}

std::unique_ptr<ReadWriteLockDelegate> makeReadWriteLockDelegate(ReadWriteLock::RecursionMode mode)
{
    static const auto impl = mutexImplementation();
    if (impl & MutexImplementations::qt)
        return std::make_unique<ReadWriteLockQtDelegate>(mode);

    if (impl & MutexImplementations::std)
        return std::make_unique<ReadWriteLockStdDelegate>(mode);

    if (impl & MutexImplementations::debug)
    {
        return std::make_unique<ReadWriteLockDebugDelegate>(
            mode, impl == MutexImplementations::analyze);
    }

    NX_ASSERT(false, "Unknown mutex implementation: %1", impl);
    return std::make_unique<ReadWriteLockQtDelegate>(mode);
}

std::unique_ptr<WaitConditionDelegate> makeWaitConditionDelegate()
{
    static const auto impl = mutexImplementation();
    if (impl & MutexImplementations::qt)
        return std::make_unique<WaitConditionQtDelegate>();

    if (impl & MutexImplementations::std)
        return std::make_unique<WaitConditionStdDelegate>();

    if (impl & MutexImplementations::debug)
        return std::make_unique<WaitConditionDebugDelegate>();

    NX_ASSERT(false, "Unknown mutex implementation: %1", impl);
    return std::make_unique<WaitConditionQtDelegate>();
}

} // namespace nx
