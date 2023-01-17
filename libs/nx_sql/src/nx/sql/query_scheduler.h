// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <condition_variable>
#include <thread>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/move_only_func.h>

namespace nx::sql {

class AbstractAsyncSqlQueryExecutor;
class QueryContext;

/**
 * Runs the given SQL query function periodically.
 */
class NX_SQL_API QueryScheduler
{
public:
    QueryScheduler(
        nx::sql::AbstractAsyncSqlQueryExecutor* queryExecutor,
        nx::utils::MoveOnlyFunc<void(nx::sql::QueryContext*)> func);

    ~QueryScheduler();

    /**
     * Execute query function with the specified delay between completion and the subsequent start.
     * @param firstRunDelay Delay before running function for the first time.
     */
    void start(
        std::chrono::milliseconds periodicDelay,
        std::chrono::milliseconds firstRunDelay = std::chrono::milliseconds::zero());

    /**
     * Stop execution.
     * Waits for the running query function to complete (if any).
     */
    void stop();

    void executeImmediately();

private:
    void threadMain();
    void runQuery();

private:
    nx::sql::AbstractAsyncSqlQueryExecutor* m_queryExecutor = nullptr;
    nx::utils::MoveOnlyFunc<void(nx::sql::QueryContext*)> m_func;
    std::chrono::milliseconds m_periodicDelay = std::chrono::milliseconds::zero();
    std::optional<std::chrono::milliseconds> m_nextShotDelay;
    nx::Mutex m_mutex;
    nx::WaitCondition m_cond;
    std::thread m_thread;
    bool m_stopped = false;
};

} // namespace nx::sql
