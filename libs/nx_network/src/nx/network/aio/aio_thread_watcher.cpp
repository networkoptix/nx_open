// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "aio_thread_watcher.h"

#include <nx/utils/log/log.h>

namespace nx::network::aio {

using namespace std::chrono;

AioThreadWatcher::AioThreadWatcher(
    int pollRatePerSecond,
    int averageTimeMultiplier,
    std::chrono::milliseconds absoluteThreshold,
    StuckThreadHandler handler)
    :
    m_absoluteThreshold(absoluteThreshold),
    m_stuckThreadHandler(std::move(handler))
{
    setObjectName("AioThreadWatcher");

    if (!NX_ASSERT(pollRatePerSecond > 0))
        pollRatePerSecond = 10;
    m_threadSleep = 1000ms / pollRatePerSecond;

    if (!NX_ASSERT(averageTimeMultiplier >= 0))
        averageTimeMultiplier = 5000;
    m_averageTimeMultiplier = averageTimeMultiplier;
}

AioThreadWatcher::~AioThreadWatcher()
{
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        pleaseStop();
        m_waitCondition.wakeOne();
    }
    wait();
}

void AioThreadWatcher::startWatcherThread(const std::vector<const AbstractAioThread*>& threadsToWatch)
{
    m_threadsToWatch.reserve(threadsToWatch.size());
    for (const auto& t: threadsToWatch)
        m_threadsToWatch.emplace_back().aioThread = t;

    start();
}

void AioThreadWatcher::setAverageTimeMultiplier(int multiplier)
{
    m_averageTimeMultiplier = multiplier;
}

int AioThreadWatcher::averageTimeMultiplier() const
{
    return m_averageTimeMultiplier.load();
}

void AioThreadWatcher::setAbsoluteThreshold(std::chrono::milliseconds absoluteThreshold)
{
    m_absoluteThreshold = absoluteThreshold;
}

std::chrono::milliseconds AioThreadWatcher::absoluteThreshold() const
{
    return m_absoluteThreshold.load();
}

void AioThreadWatcher::aboutToInvoke(const AbstractAioThread* aioThread, const char* functionType)
{
    auto& ctx = find(aioThread);
    ctx.stuck = false;
    ctx.functionType = functionType;
    ctx.watchStart = nx::utils::monotonicTime();
}

void AioThreadWatcher::doneInvoking(
    const AbstractAioThread* thread,
    std::chrono::microseconds averageExecutionTime)
{
    auto& ctx = find(thread);
    ctx.watchStart = std::chrono::steady_clock::time_point{};
    ctx.averageExecutionTime = averageExecutionTime;
    if (const auto wasStuck = ctx.stuck.exchange(false); wasStuck)
    {
        NX_DEBUG(this, "Thread is unstuck: %1, id: %2",
            ctx.aioThread, ctx.aioThread->systemThreadId());
    }
}

std::vector<AioThreadWatcher::StuckThread> AioThreadWatcher::detectStuckThreads()
{
    std::vector<StuckThread> result;
    for (auto& ctx: m_threadsToWatch)
    {
        if (auto t = detectStuckThread(&ctx))
            result.push_back(*t);
    }
    return result;
}

void AioThreadWatcher::run()
{
    NX_DEBUG(this, "Starting AIO watcher thread");

    while (!needToStop())
    {
        {
            NX_MUTEX_LOCKER lock(&m_mutex);
            m_waitCondition.waitFor(
                &m_mutex,
                m_threadSleep,
                [this](){ return needToStop() || !m_newlyStuckThreads.empty(); });
            if (needToStop())
                break;

            if (!m_newlyStuckThreads.empty())
            {
                auto newlyStuckThreads = std::exchange(m_newlyStuckThreads, {});
                lock.unlock();

                while (!newlyStuckThreads.empty())
                {
                    auto t = std::move(newlyStuckThreads.front());
                    newlyStuckThreads.pop();
                    m_stuckThreadHandler(t);
                }
            }
        }

        for (auto& ctx: m_threadsToWatch)
            detectStuckThread(&ctx);
    }

    NX_DEBUG(this, "AIO watcher thread stopped");
}

AioThreadWatcher::WatchContext& AioThreadWatcher::find(const AbstractAioThread* aioThread)
{
    auto it = std::find_if(
        m_threadsToWatch.begin(),
        m_threadsToWatch.end(),
        [&aioThread](const auto& ctx){ return ctx.aioThread == aioThread; });
    NX_CRITICAL(it != m_threadsToWatch.end());
    return *it;
}

std::optional<AioThreadWatcher::StuckThread>
    AioThreadWatcher::detectStuckThread(WatchContext* ctx)
{
    const auto watchStart = ctx->watchStart.load();
    const auto averageExecutionTime = ctx->averageExecutionTime.load();
    const auto currentExecutionTime = nx::utils::monotonicTime() - watchStart;

    if (watchStart == steady_clock::time_point{})
        return std::nullopt;

    if ((averageExecutionTime != 0us
            && m_averageTimeMultiplier.load() > 0
            && currentExecutionTime >= averageExecutionTime * m_averageTimeMultiplier.load())
        || currentExecutionTime >= m_absoluteThreshold.load())
    {
        StuckThread t{
            .aioThread = ctx->aioThread,
            .currentExecutionTime = duration_cast<microseconds>(currentExecutionTime),
            .averageExecutionTime = averageExecutionTime,
            .functionType = ctx->functionType.load(),
        };

        if (const auto alreadyStuck = ctx->stuck.exchange(true); !alreadyStuck)
        {
            NX_MUTEX_LOCKER lock{&m_mutex};
            m_newlyStuckThreads.push(t);
            m_waitCondition.wakeOne();
        }

        return t;
    }

    return std::nullopt;
}

} // namespace nx::network::aio
