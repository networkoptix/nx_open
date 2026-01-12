// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "bookmarks_backend.h"

#include <chrono>
#include <memory>

#include <QtCore/QWeakPointer>

#include <camera/camera_bookmarks_manager.h>
#include <core/resource/resource.h>
#include <nx/vms/client/core/system_context.h>

#include "data_list_helpers.h"

using namespace std::chrono;

namespace nx::vms::client::core {
namespace timeline {

namespace {

milliseconds timestamp(milliseconds source) { return source; }
milliseconds timestamp(const QnCameraBookmark& source) { return source.startTimeMs; }

} // namespace

struct BookmarksBackend::Private
{
    QWeakPointer<QnResource> resource;
};

BookmarksBackend::BookmarksBackend(const QnResourcePtr& resource):
    d(new Private{.resource = resource})
{
}

BookmarksBackend::~BookmarksBackend()
{
    // Required here for forward-declared scoped pointer destruction.
}

QnResourcePtr BookmarksBackend::resource() const
{
    return d->resource.lock();
}

QFuture<QnCameraBookmarkList> BookmarksBackend::load(const QnTimePeriod& period, int limit)
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
    const auto manager = systemContext->cameraBookmarksManager();
    if (!NX_ASSERT(manager))
        return {};

    const auto promise = std::make_shared<QPromise<QnCameraBookmarkList>>();
    promise->start();

    const auto responseHandler =
        [this, promise, period, weakGuard = this->weak_from_this()](
            bool success, rest::Handle requestId, QnCameraBookmarkList result)
        {
            const auto guard = weakGuard.lock();
            if (!guard)
                return;

            const bool canceled = promise->isCanceled();
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

            using Helpers = DataListHelpers<QnCameraBookmarkList>;

            if (!NX_ASSERT(std::is_sorted(result.cbegin(), result.cend(), Helpers::greater)))
                std::stable_sort(result.begin(), result.end(), Helpers::greater);

            // Remove bookmarks that started before the requested time period.
            Helpers::clip(result, period);
            NX_VERBOSE(this,
                "After removal of bookmarks started before the requested time period, count=%1",
                result.size());

            promise->emplaceResult(std::move(result));
            promise->finish();
        };

    const QnCameraBookmarkSearchFilter filter{
        .startTimeMs = period.startTime(),
        .endTimeMs = period.endTime() - 1ms, //< Bookmark request treats endTime as included.
        .limit = limit,
        .orderBy = common::BookmarkSortOrder{
            api::BookmarkSortField::startTime, Qt::DescendingOrder},
        .cameras = {resource->getId()}};

    const auto requestId = manager->getBookmarksAsync(filter, responseHandler, /*executor*/ {});
    if (!requestId)
    {
        NX_WARNING(this, "Failed to send request for [%1, %2), limit=%3",
            filter.startTimeMs, filter.endTimeMs, filter.limit);
        return {};
    }

    NX_VERBOSE(this, "Sent request (%1) for [%2, %3), limit=%4",
        requestId, filter.startTimeMs, filter.endTimeMs, filter.limit);

    return promise->future();
}

} // namespace timeline
} // namespace nx::vms::client::core
