// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <chrono>
#include <map>

#include <QtCore/QMetaObject>
#include <QtCore/QPromise>
#include <QtCore/QScopedPointerDeleteLater>
#include <QtCore/QThread>

#include <nx/utils/log/log.h>
#include <nx/utils/pending_operation.h>
#include <nx/utils/thread/mutex.h>

#include "abstract_proxy_backend.h"
#include "data_list_helpers.h"

namespace nx::vms::client::core {
namespace timeline {

/**
 * A proxy backend that queues incoming requests, aggregates them into consecutive blocks,
 * and requests those bigger blocks from the underlying backend.
 * As all timeline backends, assumes the data is fetched in *descending* order by time.
 */
template<typename DataList>
class AggregatingProxyBackend: public AbstractProxyBackend<DataList>
{
    using milliseconds = std::chrono::milliseconds;
    using Helpers = DataListHelpers<DataList>;

public:
    explicit AggregatingProxyBackend(const BackendPtr<DataList>& source);

    virtual QFuture<DataList> load(const QnTimePeriod& period, int limit) override;

    /**
     * A hint for the maximum aggregated block limit.
     * Aggregated block limit is calculated as a sum of individual request limits, preferably
     * not greater than `maximumLimit`, but never less than any of the individual request limits.
     * Default for `maximumLimit` is 100.
     */
    int maximumLimit() const { return m_desiredMaximumLimit; }
    void setMaximumLimit(int value) { m_desiredMaximumLimit = value; }

    /**
     * A minimum time between incoming requests required for queue processing to start.
     * Note: if the queue grows too big (over 100 requests), its processing is forced.
     */
    milliseconds aggregationInterval() const;
    void setAggregationInterval(milliseconds value);

private:
    using Future = QFuture<DataList>;
    using Promise = QPromise<DataList>;

    struct PendingRequest
    {
        Promise promise;
        int limit;
    };

    using PendingRequests = std::multimap<QnTimePeriod, PendingRequest>;
    PendingRequests m_queuedRequests;

    using RunningRequests = std::map<int /*requestId*/, PendingRequests>;
    RunningRequests m_runningRequests;

    static constexpr int kMaximumQueueSize = 100;
    const std::unique_ptr<nx::utils::PendingOperation, QScopedPointerDeleteLater> m_loadOp;

    std::atomic<int> m_desiredMaximumLimit = 100;
    nx::Mutex m_mutex{nx::Mutex::Recursive};
    int m_requestId = 0;

private:
    void processQueuedRequests();
    void requestBlock(const QnTimePeriod& block, int limit, PendingRequests requests);
    void handleDataReceived(int requestId, const QnTimePeriod& block, int limit,
        std::optional<DataList> data);
    void requestQueueProcessing();
};

// ------------------------------------------------------------------------------------------------
// Implementation.

template<typename DataList>
AggregatingProxyBackend<DataList>::AggregatingProxyBackend(const BackendPtr<DataList>& source):
    AbstractProxyBackend<DataList>(source),
    m_loadOp(new nx::utils::PendingOperation{})
{
    m_loadOp->setIntervalMs(100);
    m_loadOp->setFlags(nx::utils::PendingOperation::FireOnlyWhenIdle);
}

template<typename DataList>
QFuture<DataList> AggregatingProxyBackend<DataList>::load(const QnTimePeriod& period, int limit)
{
    if (!NX_ASSERT(!period.isEmpty() && limit > 0)
        || !this->source()
        || !NX_ASSERT(this->shared_from_this()))
    {
        return {};
    }

    NX_VERBOSE(this, "Queueing a request for [%1, %2), limit=%3",
        period.startTimeMs, period.endTimeMs(), limit);

    Promise promise;
    promise.start();
    const auto future = promise.future();

    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        m_queuedRequests.emplace(period, PendingRequest{std::move(promise), limit});
    }

    requestQueueProcessing();
    return future;
}

template<typename DataList>
std::chrono::milliseconds AggregatingProxyBackend<DataList>::aggregationInterval() const
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    return m_loadOp->interval();
}

template<typename DataList>
void AggregatingProxyBackend<DataList>::setAggregationInterval(milliseconds value)
{
    if (!NX_ASSERT(QThread::currentThread() == m_loadOp->thread()))
        return;

    NX_MUTEX_LOCKER lk(&m_mutex);
    m_loadOp->setInterval(value);
}

template<typename DataList>
void AggregatingProxyBackend<DataList>::processQueuedRequests()
{
    PendingRequests queuedRequests;
    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        std::swap(m_queuedRequests, queuedRequests);
    }

    NX_VERBOSE(this, "Processing queued requests, queue size=%1", queuedRequests.size());

    std::optional<QnTimePeriod> aggregatedBlock;
    int maxLimit = 0;
    qint64 sumLimit = 0;

    const auto aggregatedLimit =
        [&]()
        {
            // Aggregated block limit is calculated as a sum of individual request limits,
            // preferably not greater than `maximumLimit`, but never less than any of
            // the individual request limits.
            return std::min(sumLimit, (qint64) std::max(maxLimit, m_desiredMaximumLimit.load()));
        };

    const auto intersectsWithAggregatedBlock =
        [&](const QnTimePeriod& period)
        {
            // Time periods are sorted by `startTimeMs` in ascending order, and are processed in
            // that order, so only one side comparison is reqiured.
            return aggregatedBlock && period.startTimeMs <= aggregatedBlock->endTimeMs();
        };

    for (auto it = queuedRequests.begin(); it != queuedRequests.end(); )
    {
        const auto& [period, request] = *it;

        if (request.promise.isCanceled())
        {
            NX_VERBOSE(this, "Request for [%1, %2), limit=%3 was canceled",
                period.startTimeMs, period.endTimeMs(), request.limit);
            it = queuedRequests.erase(it);
            continue;
        }

        // If currently aggregated block exists and the next time period intersects with it...
        if (intersectsWithAggregatedBlock(period))
        {
            // Add it to the aggregated block.
            aggregatedBlock->setEndTimeMs(
                std::max(aggregatedBlock->endTimeMs(), period.endTimeMs()));

            maxLimit = std::max(maxLimit, request.limit);
            sumLimit += request.limit;
        }
        // Otherwise...
        else
        {
            // If currently aggregated block exists, finish & process it.
            if (aggregatedBlock
                && NX_ASSERT(aggregatedBlock->isValid() && it != queuedRequests.begin()))
            {
                requestBlock(*aggregatedBlock, aggregatedLimit(), PendingRequests{
                    std::make_move_iterator(queuedRequests.begin()), std::make_move_iterator(it)});
                it = queuedRequests.erase(queuedRequests.begin(), it);
            }

            // Start a new aggregated block.
            aggregatedBlock = period;
            maxLimit = request.limit;
            sumLimit = request.limit;
        }

        ++it;
    }

    // Finish & process the last aggregated block, if it exists.
    if (aggregatedBlock && NX_ASSERT(aggregatedBlock->isValid() && !queuedRequests.empty()))
        requestBlock(*aggregatedBlock, aggregatedLimit(), std::move(queuedRequests));
}

template<typename DataList>
void AggregatingProxyBackend<DataList>::requestBlock(
    const QnTimePeriod& block, int limit, PendingRequests requests)
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    const int requestId = m_requestId++;
    m_runningRequests.emplace(requestId, std::move(requests));

    NX_VERBOSE(this, "Sending aggregated request (%1) for [%2, %3), limit=%4, sources=%5",
        requestId, block.startTimeMs, block.endTimeMs(), limit, requests.size());

    this->source()->load(block, limit)
        .then(
            [this, requestId, block, limit, weakGuard = this->weak_from_this()](DataList result)
            {
                if (weakGuard.lock())
                    handleDataReceived(requestId, block, limit, std::move(result));
            })
        .onCanceled(
            [this, requestId, block, limit, weakGuard = this->weak_from_this()]()
            {
                if (weakGuard.lock())
                    handleDataReceived(requestId, block, limit, std::nullopt);
            });
}

template<typename DataList>
void AggregatingProxyBackend<DataList>::handleDataReceived(
    int requestId, const QnTimePeriod& block, int limit, std::optional<DataList> data)
{
    PendingRequests requests;
    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        auto it = m_runningRequests.find(requestId);
        if (!NX_ASSERT(it != m_runningRequests.end()))
            return;

        requests = std::move(it->second);
        m_runningRequests.erase(it);
    }

    if (!data)
    {
        NX_WARNING(this, "Aggregated request (%1) for [%2, %3) failed", requestId,
            block.startTimeMs, block.endTimeMs());
        return;
    }

    // If received response is potentially clipped (received not less than `limit` items),
    // the earlest (since the sort order is descending) millisecond may be incomplete (as there
    // may be more items in that millisecond which we didn't receive because of the limit).
    // In that case the earliest reliable (complete) millisecond is not `block.startTime()`,
    // but the next millisecond after the received earliest timestamp, which is at the *back*
    // of `data` (as it's sorted in descending order).

    const bool clipped = (int) data->size() == limit;
    const milliseconds reliableStartTime = clipped
        ? Helpers::timestamp(*std::prev(data->end())) + milliseconds(1)
        : block.startTime();

    NX_VERBOSE(this,
        "Received response for [%1, %2), limit=%3, received=%4, covered startTimeMs=%5",
        block.startTimeMs, block.endTimeMs(), limit, data->size(), reliableStartTime.count());

    int processed = 0;
    for (auto it = requests.begin(); it != requests.end(); )
    {
        auto& [period, request] = *it;

        if (request.promise.isCanceled())
        {
            NX_VERBOSE(this, "Request for [%1, %2), limit=%3 was canceled",
                period.startTimeMs, period.endTimeMs(), request.limit);
            it = requests.erase(it);
            continue;
        }

        auto sliceForRequest = Helpers::slice(*data, period, request.limit);

        if (NX_ASSERT(block.contains(period) && request.limit <= limit)
            && (period.endTime() == block.endTime()
                || period.startTime() >= reliableStartTime
                || request.limit == (int) sliceForRequest.size()))
        {
            NX_VERBOSE(this, "Request for [%1, %2), limit=%3 is fulfilled",
                period.startTimeMs, period.endTimeMs(), request.limit);
            ++processed;
            request.promise.emplaceResult(std::move(sliceForRequest));
            request.promise.finish();
            it = requests.erase(it);
            continue;
        }

        ++it;
    }

    if (requests.empty())
    {
        // If we processed all periods, there's nothing more to do.
        NX_VERBOSE(this, "Fulfilled %1 of %1 requests", processed);
    }
    else if (processed <= 1)
    {
        // If we processed no more than one period, request the rest without aggregating.
        NX_VERBOSE(this, "Fulfilled %1 of %2 requests, sending the rest without aggregation",
            processed, processed + (int) requests.size());

        for (auto& [period, request]: requests)
        {
            PendingRequests map;
            map.emplace(period, std::move(request));
            requestBlock(period, request.limit, std::move(map));
        }
    }
    else
    {
        // If we processed more than one period, just queue the rest for aggregation again.
        NX_VERBOSE(this, "Fulfilled %1 of %2 requests, re-queueing the rest",
            processed, processed + (int) requests.size());

        {
            NX_MUTEX_LOCKER lk(&m_mutex);

            m_queuedRequests.insert(
                std::make_move_iterator(requests.begin()), std::make_move_iterator(requests.end()));
        }

        requestQueueProcessing();
    }
}

template<typename DataList>
void AggregatingProxyBackend<DataList>::requestQueueProcessing()
{
    QMetaObject::invokeMethod(m_loadOp.get(),
        [this, weakGuard = this->weak_from_this()]()
        {
            const auto guard = weakGuard.lock();
            if (!guard || !NX_ASSERT(QThread::currentThread() == m_loadOp->thread()))
                return;

            NX_MUTEX_LOCKER lk(&m_mutex);

            // We cannot set the callback in the constructor, because there's no shared pointer
            // to `this` there. So just set it here every time, it's safe.
            m_loadOp->setCallback(
                [this, weakGuard]()
                {
                    if (weakGuard.lock())
                        processQueuedRequests();
                });

            m_loadOp->requestOperation();

            if ((int) m_queuedRequests.size() > kMaximumQueueSize)
            {
                NX_VERBOSE(this, "Queue size (%1) exceeded maximum limit (%2), forcing processing",
                    m_queuedRequests.size(), kMaximumQueueSize);

                m_loadOp->fire();
            }
        },
        Qt::AutoConnection);
}

} // namespace timeline
} // namespace nx::vms::client::core
