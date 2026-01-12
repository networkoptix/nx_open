// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "bookmark_loader_delegate.h"

#include <memory>

#include <QtCore/QFutureWatcher>
#include <QtCore/QPointer>
#include <QtCore/QPromise>

#include <core/resource/camera_bookmark.h>
#include <core/resource/resource.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/range_adapters.h>

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
    const BackendPtr backend;
    const int maxBookmarksPerBucket;

    static void handleLoadingFinished(
        QPromise<MultiObjectData>& promise,
        const QnTimePeriod& period,
        milliseconds minimumStackDuration,
        int limit,
        const nx::Uuid& cameraId,
        QnCameraBookmarkList result)
    {
        QnTimePeriodList chunks;
        if ((int) result.size() > limit)
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
                .iconPaths = {"image://skin/20x20/Outline/bookmark.svg"},
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
                .iconPaths = {"image://skin/20x20/Outline/bookmark.svg"},
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
    const BackendPtr& backend,
    int maxBookmarksPerBucket,
    QObject* parent)
    :
    AbstractLoaderDelegate(parent),
    d(new Private{.backend = backend, .maxBookmarksPerBucket = maxBookmarksPerBucket})
{
    NX_ASSERT(d->backend);
}

BookmarkLoaderDelegate::~BookmarkLoaderDelegate()
{
    // Required here for forward-declared scoped pointer destruction.
}

QFuture<MultiObjectData> BookmarkLoaderDelegate::load(
    const QnTimePeriod& period, milliseconds minimumStackDuration)
{
    if (!d->backend || period.isEmpty())
        return {};

    // `maxBookmarksPerBucket + 1` limit should be requested to see if there's exactly
    // `maxBookmarksPerBucket` or more bookmarks in the requested `period`.
    const auto nativeDataFuture = d->backend->load(period, d->maxBookmarksPerBucket + 1);
    if (nativeDataFuture.isCanceled())
        return {};

    using Watcher = QFutureWatcher<QnCameraBookmarkList>;
    const auto nativeDataReadinessWatcher = new Watcher(this);

    QPromise<MultiObjectData> resultPromise;
    resultPromise.start();
    const auto resultFuture = resultPromise.future();

    connect(nativeDataReadinessWatcher, &Watcher::finished, this,
        [this, nativeDataReadinessWatcher, resultPromise = std::move(resultPromise), period,
            minimumStackDuration]() mutable
        {
            const auto resource = d->backend->resource();
            if (resource
                && !resultPromise.isCanceled()
                && nativeDataReadinessWatcher->future().isValid()
                && nativeDataReadinessWatcher->future().isResultReadyAt(0))
            {
                Private::handleLoadingFinished(resultPromise, period, minimumStackDuration,
                    d->maxBookmarksPerBucket, resource->getId(),
                    nativeDataReadinessWatcher->future().takeResult());
            }

            resultPromise.finish();
            nativeDataReadinessWatcher->deleteLater();
        });

    nativeDataReadinessWatcher->setFuture(nativeDataFuture);
    return resultFuture;
}

} // namespace timeline
} // namespace nx::vms::client::mobile
