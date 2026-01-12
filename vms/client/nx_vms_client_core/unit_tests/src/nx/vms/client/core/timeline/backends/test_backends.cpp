// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "test_backends.h"

#include <QtCore/QPromise>
#include <QtCore/QThreadPool>

namespace nx::vms::client::core::timeline {
namespace test {

QFuture<TestDataList> AsyncTestBackend::load(const QnTimePeriod& period, int limit)
{
    if (m_beforeLoadHook)
        m_beforeLoadHook();

    log().push(period, limit);

    QPromise<TestDataList> promise;
    const auto future = promise.future();
    promise.start();

    // For simplitity and destruction order safety in tests, just capture a copy of `m_data`.
    QThreadPool::globalInstance()->start(
        [data = m_data, failure = failureMode(), period, limit,
            promise = std::move(promise)]() mutable
        {
            if (failure)
                return;

            promise.emplaceResult(AsyncTestBackend::slice(data, period, limit));
            promise.finish();
        });

    return future;
}

QFuture<TestDataList> SyncTestBackend::load(const QnTimePeriod& period, int limit)
{
    if (m_beforeLoadHook)
        m_beforeLoadHook();

    log().push(period, limit);

    if (failureMode())
        return {};

    QPromise<TestDataList> promise;
    promise.start();
    promise.emplaceResult(SyncTestBackend::slice(m_data, period, limit));
    promise.finish();
    return promise.future();
}

} // namespace test
} // namespace nx::vms::client::core::timeline
