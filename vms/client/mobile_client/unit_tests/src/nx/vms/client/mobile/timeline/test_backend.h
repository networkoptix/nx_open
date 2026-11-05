// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <algorithm>
#include <optional>
#include <vector>

#include <QtCore/QEventLoop>
#include <QtCore/QFutureWatcher>
#include <QtCore/QPromise>
#include <QtCore/QTimer>

#include <core/resource/resource.h>
#include <nx/vms/client/core/timeline/backends/abstract_backend.h>
#include <recording/time_period.h>

namespace nx::vms::client::mobile::timeline::test {

struct Request
{
    QnTimePeriod period;
    int limit = 0;
};

/**
 * Returns up to `limit` first items of preset data either immediately (`ready` mode), on demand
 * via `finish()` (`deferred` mode), or returns a canceled future (`canceled` mode).
 */
template<typename DataList>
class TestBackend: public core::timeline::AbstractBackend<DataList>
{
public:
    enum class Mode
    {
        ready,
        deferred,
        canceled
    };

    TestBackend(const QnResourcePtr& resource, DataList data, Mode mode = Mode::ready):
        m_resource(resource),
        m_data(std::move(data)),
        m_mode(mode)
    {
    }

    virtual QnResourcePtr resource() const override { return m_resource; }

    virtual QFuture<DataList> load(const QnTimePeriod& period, int limit) override
    {
        requests.push_back({period, limit});

        const auto size = std::min<qsizetype>(limit, (qsizetype) m_data.size());
        DataList result(m_data.begin(), m_data.begin() + size);

        QPromise<DataList> promise;
        auto future = promise.future();
        promise.start();

        switch (m_mode)
        {
            case Mode::ready:
                promise.addResult(std::move(result));
                promise.finish();
                break;

            case Mode::deferred:
                m_pending = std::move(promise);
                m_pendingResult = std::move(result);
                break;

            case Mode::canceled:
                future.cancel();
                promise.finish();
                break;
        }

        return future;
    }

    bool isPendingCanceled() const { return m_pending && m_pending->isCanceled(); }

    void finish()
    {
        if (!m_pending)
            return;

        m_pending->addResult(std::move(m_pendingResult));
        m_pending->finish();
        m_pending.reset();
    }

    std::vector<Request> requests;

private:
    const QnResourcePtr m_resource;
    const DataList m_data;
    const Mode m_mode;
    std::optional<QPromise<DataList>> m_pending;
    DataList m_pendingResult;
};

/** Returns false if the future isn't finished in `timeout`. */
template<typename T>
bool waitForCompletion(
    const QFuture<T>& future, std::chrono::milliseconds timeout = std::chrono::seconds(1))
{
    QEventLoop eventLoop;
    QFutureWatcher<T> watcher;

    QObject::connect(&watcher, &QFutureWatcher<T>::finished, &eventLoop, &QEventLoop::quit);
    QTimer::singleShot(timeout, &eventLoop, &QEventLoop::quit);
    watcher.setFuture(future);

    if (!future.isFinished())
        eventLoop.exec();

    return future.isFinished();
}

} // namespace nx::vms::client::mobile::timeline::test
