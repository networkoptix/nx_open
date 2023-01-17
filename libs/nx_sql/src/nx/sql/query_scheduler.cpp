// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "query_scheduler.h"

#include "async_sql_query_executor.h"
#include "query_context.h"

namespace nx::sql {

QueryScheduler::QueryScheduler(
    nx::sql::AbstractAsyncSqlQueryExecutor* queryExecutor,
    nx::utils::MoveOnlyFunc<void(nx::sql::QueryContext*)> func)
    :
    m_queryExecutor(queryExecutor),
    m_func(std::move(func))
{
}

QueryScheduler::~QueryScheduler()
{
    stop();
}

void QueryScheduler::start(
    std::chrono::milliseconds periodicDelay,
    std::chrono::milliseconds firstRunDelay)
{
    NX_VERBOSE(this, "Starting query sceduler with periodic delay %1 and initial delay %2",
        periodicDelay, firstRunDelay);

    m_stopped = false;
    m_periodicDelay = periodicDelay;
    m_nextShotDelay = firstRunDelay;

    m_thread = std::thread([this]() { threadMain(); });
}

void QueryScheduler::stop()
{
    if (!m_thread.joinable())
        return;

    NX_VERBOSE(this, "Stopping query scheduler");

    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_stopped = true;
        m_cond.wakeOne();
    }

    m_thread.join();
}

void QueryScheduler::executeImmediately()
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    m_nextShotDelay = std::chrono::milliseconds::zero();
    m_cond.wakeOne();
}

void QueryScheduler::threadMain()
{
    using namespace std::chrono;

    NX_VERBOSE(this, "QueryScheduler thread started");

    while (!m_stopped)
    {
        NX_MUTEX_LOCKER lock(&m_mutex);

        milliseconds delay = m_periodicDelay;
        if (m_nextShotDelay)
        {
            delay = *m_nextShotDelay;
            m_nextShotDelay.reset();
        }

        // Waiting.
        for (auto nextQueryTime = nx::utils::monotonicTime() + delay;
            !m_stopped && !m_nextShotDelay;
            )
        {
            auto curTime = nx::utils::monotonicTime();
            if (nextQueryTime <= curTime)
                break;  // Time to run the query.

            if (nextQueryTime - curTime > delay) // clock skew?
                nextQueryTime = curTime + delay;

            m_cond.wait(&m_mutex, floor<milliseconds>(nextQueryTime - curTime));
        }

        if (m_stopped)
            break;
        m_nextShotDelay.reset();

        lock.unlock();

        runQuery();
    }

    NX_VERBOSE(this, "QueryScheduler thread stopped");
}

void QueryScheduler::runQuery()
{
    try
    {
        m_queryExecutor->executeUpdateSync(
            [this](nx::sql::QueryContext* ctx) { m_func(ctx); });

        NX_VERBOSE(this, "Query succeeded");
    }
    catch (const Exception& e)
    {
        NX_DEBUG(this, "Query failed with %1", e.dbResult());
    }
}

} // namespace nx::sql
