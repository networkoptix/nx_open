// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <queue>

#include "aio_thread.h"

namespace nx::network::aio {

/**
 * Watches for stuck AIO threads.
 * A thread is considered stuck when its execution time exceeds either its average execution time
 * multiplied by some factor or an absolute time threshold.
 */
class NX_NETWORK_API AioThreadWatcher: private nx::utils::Thread
{
public:
    struct StuckThread
    {
        const AbstractAioThread* aioThread = nullptr;
        std::chrono::microseconds currentExecutionTime;
        std::chrono::microseconds averageExecutionTime;
        std::string_view functionType;

        QString toString() const
        {
            return nx::format(
                "%1, id: %2, functionType: %3, currentExecutionTime: %4,"
                    " averageExecutionTime: %5",
                aioThread, aioThread->systemThreadId(), functionType, currentExecutionTime,
                averageExecutionTime);
        }
    };

    using StuckThreadHandler = nx::utils::MoveOnlyFunc<void(StuckThread)>;

    AioThreadWatcher(
        int pollRatePerSecond,
        int averageTimeMultiplier,
        std::chrono::milliseconds absoluteThreshold,
        StuckThreadHandler handler);
    ~AioThreadWatcher();

    // Functions below are thread safe.

    void startWatcherThread(const std::vector<const AbstractAioThread*>& threadsToWatch);

    using nx::utils::Thread::stop;

    void setAverageTimeMultiplier(int multiplier);
    int averageTimeMultiplier() const;

    void setAbsoluteThreshold(std::chrono::milliseconds absoluteThreshold);
    std::chrono::milliseconds absoluteThreshold() const;

    /**
     * Get a list of stuck threads. Performs the same computation as internal watcher thread.
     */
    std::vector<StuckThread> detectStuckThreads();

    void aboutToInvoke(const AbstractAioThread* thread, const char* functionType);
    void doneInvoking(
        const AbstractAioThread* thread,
        std::chrono::microseconds averageExecutionTime);

private:
    struct WatchContext
    {
        const AbstractAioThread* aioThread = nullptr;
        std::atomic<std::chrono::steady_clock::time_point> watchStart;
        std::atomic<std::chrono::microseconds> averageExecutionTime;
        std::atomic<const char*> functionType = nullptr;
        std::atomic_bool stuck = false;

        WatchContext() = default;

        WatchContext(const WatchContext& other):
            aioThread(other.aioThread),
            watchStart(other.watchStart.load()),
            averageExecutionTime(other.averageExecutionTime.load()),
            functionType(other.functionType.load()),
            stuck(other.stuck.load())
        {
        }
    };

    virtual void run() override;

    WatchContext& find(const AbstractAioThread* aioThread);
    /**
     * @return a value if thread is stuck.
     * @param newlyStuck set to true if the thread watch transitioned from not stuck to stuck in
     * this call.
     */
    std::optional<StuckThread> detectStuckThread(WatchContext* ctx);

private:
    std::chrono::milliseconds m_threadSleep;
    std::atomic_int m_averageTimeMultiplier;
    std::atomic<std::chrono::milliseconds> m_absoluteThreshold;
    StuckThreadHandler m_stuckThreadHandler;
    std::vector<WatchContext> m_threadsToWatch;

    nx::Mutex m_mutex;
    nx::WaitCondition m_waitCondition;
    std::queue<StuckThread> m_newlyStuckThreads;
};

} // namespace nx::network::aio
