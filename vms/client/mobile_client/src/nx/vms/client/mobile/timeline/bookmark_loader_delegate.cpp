// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "bookmark_loader_delegate.h"

#include <memory>

#include <QtCore/QPointer>
#include <QtCore/QPromise>
#include <QtCore/QWeakPointer>

#include <camera/camera_bookmarks_manager.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource/resource.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/range_adapters.h>
#include <nx/vms/client/core/system_context.h>

namespace nx::vms::client::mobile {
namespace timeline {

using namespace std::chrono;

namespace {

QString bookmarkTitle(const QnCameraBookmark& bookmark)
{
    return bookmark.name.isEmpty()
        ? BookmarkLoaderDelegate::tr("Bookmark")
        : bookmark.name;
}

} // namespace

struct BookmarkLoaderDelegate::Private
{
    const QWeakPointer<QnResource> resource;
    const int maxBookmarksPerBucket;

    static void handleLoadingFinished(
        core::SystemContext* systemContext,
        QPromise<MultiObjectData>& promise,
        const QnTimePeriod& period,
        milliseconds minimumStackDuration,
        int limit,
        const nx::Uuid& cameraId,
        QnCameraBookmarkList&& result)
    {
        QnTimePeriodList chunks;
        if (result.size() > limit)
        {
            // If there are more than `limit` bookmarks, make one chunk covering the entire bucket.
            chunks.push_back(period);
        }
        else
        {
            chunks.reserve(result.size());

            for (auto it = result.crbegin(); it != result.crend(); ++it)
                chunks.push_back(QnTimePeriod(it->startTimeMs, it->durationMs).intersected(period));
        }

        // Remove bookmarks that started before the requested time period.
        // `result` is sorted by `QnCameraBookmark::statTimeMs` in descending order.
        const auto newEnd = std::upper_bound(result.begin(), result.end(), period.startTime(),
            [](milliseconds time, const QnCameraBookmark& bookmark)
            {
                return time > bookmark.startTimeMs;
            });

        result.erase(newEnd, result.end());

        const int count = (int) result.size();
        if (count == 1)
        {
            const auto& bookmark = result.front();

            ObjectData perObjectData{
                .id = bookmark.guid,
                .startTimeMs = bookmark.startTimeMs.count(),
                .durationMs = bookmark.durationMs.count(),
                .title = bookmarkTitle(bookmark),
                .description = bookmark.description,
                .imagePath = makeImageRequest(cameraId, bookmark.startTimeMs.count(),
                    kHighImageResolution),
                .tags = QStringList{bookmark.tags.cbegin(), bookmark.tags.cend()}};

            promise.emplaceResult(MultiObjectData{
                .caption = bookmarkTitle(bookmark),
                .description = bookmark.description,
                .iconPaths = {"image://skin/20x20/Solid/bookmark.svg"},
                .imagePaths = {makeImageRequest(cameraId, bookmark.startTimeMs.count(),
                    kLowImageResolution)},
                .positionMs = bookmark.startTimeMs.count(),
                .durationMs = bookmark.durationMs.count(),
                .count = 1,
                .objectChunks = chunks,
                .perObjectData = {perObjectData}});
        }
        else if (count > 1)
        {
            const auto title = count <= limit
                ? BookmarkLoaderDelegate::tr("Bookmarks (%n)", "", count)
                : BookmarkLoaderDelegate::tr("Bookmarks (>%n)", "", limit);

            const auto firstPosition = count <= limit
                ? result.back().startTimeMs
                : milliseconds(period.startTimeMs); //< If result is cropped, just use period start.

            const auto duration = result.front().startTimeMs - firstPosition;

            QList<ObjectData> perObjectData;
            if (duration < minimumStackDuration)
            {
                for (const auto& bookmark: nx::utils::reverseRange(result))
                {
                    perObjectData.push_back(ObjectData{
                        .id = bookmark.guid,
                        .startTimeMs = bookmark.startTimeMs.count(),
                        .durationMs = bookmark.durationMs.count(),
                        .title = bookmarkTitle(bookmark),
                        .description = bookmark.description,
                        .imagePath = makeImageRequest(cameraId, bookmark.startTimeMs.count(),
                            kHighImageResolution),
                        .tags = QStringList{bookmark.tags.cbegin(), bookmark.tags.cend()}});
                }
            }

            QStringList imagePaths;
            for (const auto& bookmark: result)
            {
                imagePaths.push_back(
                    makeImageRequest(cameraId, bookmark.startTimeMs.count(), kLowImageResolution));
                if (imagePaths.size() >= kMaxPreviewImageCount)
                    break;
            }

            promise.emplaceResult(MultiObjectData{
                .caption = title,
                .iconPaths = {"image://skin/20x20/Solid/bookmark.svg"},
                .imagePaths = imagePaths,
                .positionMs = firstPosition.count(),
                .durationMs = duration.count(),
                .count = count,
                .objectChunks = chunks,
                .perObjectData = perObjectData});
        }
        else if (!chunks.empty())
        {
            promise.emplaceResult(MultiObjectData{
                .objectChunks = chunks});
        }
    }
};

BookmarkLoaderDelegate::BookmarkLoaderDelegate(
    const QnResourcePtr& resource,
    int maxBookmarksPerBucket,
    QObject* parent)
    :
    AbstractLoaderDelegate(parent),
    d(new Private{.resource = resource, .maxBookmarksPerBucket = maxBookmarksPerBucket})
{
}

BookmarkLoaderDelegate::~BookmarkLoaderDelegate()
{
    // Required here for forward-declared scoped pointer destruction.
}

QFuture<MultiObjectData> BookmarkLoaderDelegate::load(
    const QnTimePeriod& period, milliseconds minimumStackDuration) const
{
    const auto resource = d->resource.lock();
    if (!resource || period.isEmpty())
        return {};

    const QPointer<core::SystemContext> systemContext =
        resource->systemContext()->as<core::SystemContext>();

    const auto manager = systemContext->cameraBookmarksManager();
    if (!NX_ASSERT(manager))
        return {};

    const auto promise = std::make_shared<QPromise<MultiObjectData>>();

    const int limit = d->maxBookmarksPerBucket;
    const auto cameraId = resource->getId();

    auto responseHandler = nx::utils::guarded(this,
        [systemContext, promise, period, minimumStackDuration, limit, cameraId](
            bool success,
            rest::Handle /*handle*/,
            QnCameraBookmarkList result)
        {
            if (success && systemContext)
            {
                Private::handleLoadingFinished(systemContext, *promise, period,
                    minimumStackDuration, limit, cameraId, std::move(result));
            }
            promise->finish();
        });

    const QnCameraBookmarkSearchFilter filter{
        .startTimeMs = period.startTime(),
        .endTimeMs = period.endTime() - 1ms, //< Bookmark request treats endTime as included.
        .limit = d->maxBookmarksPerBucket + 1,
        .orderBy = common::BookmarkSortOrder{
            api::BookmarkSortField::startTime, Qt::DescendingOrder},
        .cameras = {resource->getId()}};

    if (!manager->getBookmarksAsync(filter, responseHandler))
        return {};

    promise->start();
    return promise->future();
}

} // namespace timeline
} // namespace nx::vms::client::mobile
