// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_backend.h"

#include <chrono>
#include <memory>

#include <QtCore/QWeakPointer>

#include <api/server_rest_connection.h>
#include <core/resource/resource.h>
#include <nx/vms/client/core/system_context.h>

#include "data_list_helpers.h"

using namespace std::chrono;
using namespace nx::analytics::db;

namespace nx::vms::client::core {
namespace timeline {

namespace {

microseconds timestamp(microseconds source) { return source; }

microseconds timestamp(const ObjectTrackEx& source)
{
    return microseconds{source.firstAppearanceTimeUs};
}

} // namespace

struct AnalyticsBackend::Private
{
    QWeakPointer<QnResource> resource;
};

AnalyticsBackend::AnalyticsBackend(const QnResourcePtr& resource):
    d(new Private{.resource = resource})
{
}

AnalyticsBackend::~AnalyticsBackend()
{
    // Required here for forward-declared scoped pointer destruction.
}

QnResourcePtr AnalyticsBackend::resource() const
{
    return d->resource.lock();
}

QFuture<ObjectTrackList> AnalyticsBackend::load(const QnTimePeriod& period, int limit)
{
    const auto resource = d->resource.lock();
    if (!resource
        || !NX_ASSERT(resource->systemContext())
        || !NX_ASSERT(!period.isEmpty() && limit > 0)
        || !NX_ASSERT(this->shared_from_this()))
    {
        return {};
    }

    const auto systemContext = resource->systemContext()->as<core::SystemContext>();
    const auto api = systemContext->connectedServerApi();
    if (!NX_ASSERT(api))
        return {};

    QPromise<ObjectTrackList> promise;
    promise.start();

    auto future = promise.future();

    auto responseHandler =
        [this, promise = std::move(promise), period, weakGuard = this->weak_from_this()](
            bool success, rest::Handle requestId, ObjectTrackList result) mutable
        {
            const auto guard = weakGuard.lock();
            if (!guard)
                return;

            const bool canceled = promise.isCanceled();
            if (success)
            {
                NX_VERBOSE(this, "Received response (%1) for [%2, %3): count=%4 (was canceled=%5)",
                    requestId, period.startTimeMs, period.endTimeMs(), result.size(), canceled);
            }
            else
            {
                NX_WARNING(this, "Request (%1) for [%2, %3) failed (was canceled=%4)", requestId,
                    period.startTimeMs, period.endTimeMs(), canceled);
            }

            if (!success || canceled)
                return;

            using Helpers = DataListHelpers<ObjectTrackList>;

            if (!NX_ASSERT(std::is_sorted(result.cbegin(), result.cend(), Helpers::greater)))
                std::stable_sort(result.begin(), result.end(), Helpers::greater);

            // Remove tracks that started before the requested time period.
            Helpers::clip(result, period);
            NX_VERBOSE(this,
                "After removal of tracks started before the requested time period, count=%1",
                result.size());

            promise.emplaceResult(std::move(result));
            promise.finish();
        };

    Filter filter;
    filter.deviceIds.insert(resource->getId());
    filter.maxObjectTracksToSelect = limit;
    filter.sortOrder = Qt::DescendingOrder;

    // Object track request treats timePeriod.endTimeMs as included, so we need to cut the last ms.
    filter.timePeriod = QnTimePeriod{period.startTime(), period.duration() - 1ms};

    const auto requestId = api->lookupObjectTracks(filter, /*isLocal*/ false,
        std::move(responseHandler), {});
    if (!requestId)
    {
        NX_WARNING(this, "Failed to send request for [%1, %2), limit=%3",
            filter.timePeriod.startTimeMs, filter.timePeriod.endTimeMs(),
            filter.maxObjectTracksToSelect);
        return {};
    }

    NX_VERBOSE(this, "Sent request (%1) for [%2, %3), limit=%4", requestId,
        filter.timePeriod.startTimeMs, filter.timePeriod.endTimeMs(),
        filter.maxObjectTracksToSelect);

    return future;
}

} // namespace timeline
} // namespace nx::vms::client::core
