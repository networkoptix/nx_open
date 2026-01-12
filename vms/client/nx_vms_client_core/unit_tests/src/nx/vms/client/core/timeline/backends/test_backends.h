// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <optional>
#include <queue>
#include <string>
#include <vector>

#include <QtCore/QFutureWatcher>
#include <QtCore/QEventLoop>

#include <recording/time_period.h>
#include <nx/reflect/instrument.h>
#include <nx/reflect/json.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/client/core/timeline/backends/abstract_backend.h>
#include <nx/vms/client/core/timeline/backends/data_list_helpers.h>

namespace nx::vms::client::core::timeline {
namespace test {

/**
 * Test data list.
 */
struct TestData
{
    std::chrono::milliseconds timestamp;
    char payload;

    std::weak_ordering operator<=>(const TestData&) const = default;
};

using TestDataList = std::vector<TestData>;

inline std::string payload(const TestDataList& list)
{
    std::string result;
    std::transform(list.begin(), list.end(), std::back_inserter(result),
        [](const TestData& item) { return item.payload; });
    return result;
}

} // namespace test

template<>
struct DataListTraits<test::TestDataList>
{
    static std::chrono::milliseconds timestamp(const auto& item) { return item.timestamp; }
};

namespace test {

/**
 * Request log.
 */
struct Request
{
    QnTimePeriod period{};
    int limit{0};

    std::weak_ordering operator <=> (const Request&) const = default;
};
NX_REFLECTION_INSTRUMENT(Request, (period)(limit))

class RequestLog
{
public:
    int size() const
    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        return (int) m_log.size();
    }

    bool empty() const { return size() == 0; }

    void clear()
    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        m_log = {};
    }

    void push(const QnTimePeriod& period, int limit)
    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        m_log.push({period, limit});
    }

    std::optional<Request> pop()
    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        if (m_log.empty())
            return {};

        Request result = m_log.front();
        m_log.pop();
        return result;
    }

private:
    std::queue<Request> m_log;
    mutable nx::Mutex m_mutex;
};

/**
 * A base class for timeline testing backends.
 * Contains a data storage, populated in the constructor.
 */
class AbstractTestBackend: public AbstractBackend<TestDataList>
{
public:
    explicit AbstractTestBackend(TestDataList data): m_data(std::move(data)) {}
    virtual QnResourcePtr resource() const override { return {}; }

    RequestLog& log() { return m_log; }

    // If some action should be executed in a proxy backend test while its source backend is
    // processing a proxied request, set the before load hook in the source backend.
    using Hook = std::function<void()>;
    void setBeforeLoadHook(Hook value) { m_beforeLoadHook = value; }

    void setFailureMode(bool value) { m_failureMode = value; }
    bool failureMode() const { return m_failureMode.load(); }

protected:
    TestDataList m_data;
    Hook m_beforeLoadHook;

private:
    RequestLog m_log;
    std::atomic_bool m_failureMode;
};

/**
 * Testing backend, returning the requested subset of data asynchronously from another thread.
 */
class AsyncTestBackend:
    public AbstractTestBackend,
    public DataListHelpers<TestDataList>
{
public:
    explicit AsyncTestBackend(TestDataList data): AbstractTestBackend(std::move(data)) {}
    virtual QFuture<TestDataList> load(const QnTimePeriod& period, int limit) override;
};

/**
 * Testing backend, returning the requested subset of data synchronously.
 */
class SyncTestBackend:
    public AbstractTestBackend,
    public DataListHelpers<TestDataList>
{
public:
    explicit SyncTestBackend(TestDataList data): AbstractTestBackend(std::move(data)) {}
    virtual QFuture<TestDataList> load(const QnTimePeriod& period, int limit) override;
};

// ------------------------------------------------------------------------------------------------
// Utilities.

constexpr int kUnlimited = std::numeric_limits<int>::max();

/**
 * Interval [`first`, `last`], with `last` included.
 */
inline QnTimePeriod interval(std::chrono::milliseconds last, std::chrono::milliseconds first)
{
    if (last < first)
        std::swap(first, last);
    return QnTimePeriod::fromInterval(first, last + std::chrono::milliseconds(1));
}

/**
 * Wait for asynchronous operation completion without blocking.
 */
template<typename T>
void waitForCompletion(const QFuture<T>& future)
{
    QEventLoop eventLoop;
    QFutureWatcher<T> watcher;

    QObject::connect(&watcher, &QFutureWatcher<T>::finished, &eventLoop, &QEventLoop::quit);
    watcher.setFuture(future);

    if (!future.isCanceled() && !future.isFinished())
        eventLoop.exec();
}

inline std::ostream& operator<<(std::ostream& stream, const Request& request)
{
    return stream << nx::reflect::json::serialize(request);
}

} // namespace test
} // namespace nx::vms::client::core::timeline
