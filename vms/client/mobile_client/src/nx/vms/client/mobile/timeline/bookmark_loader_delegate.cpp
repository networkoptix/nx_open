// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "bookmark_loader_delegate.h"

#include <QtCore/QPointer>

#include <core/resource/camera_bookmark.h>
#include <core/resource/resource.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/range_adapters.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/mobile/timeline/bookmark_data_helper.h>

namespace nx::vms::client::mobile {
namespace timeline {

using namespace std::chrono;

namespace {

MultiObjectData merge(std::span<const common::CameraBookmark> bookmarks,
    const QnTimePeriod& period,
    milliseconds minimumStackDuration,
    int limit,
    const QnResourcePtr& resource)
{
    const int count = (int) bookmarks.size();
    if (count == 0)
        return {};

    if (count == 1)
    {
        const auto& bookmark = bookmarks.front();
        auto perObjectData = BookmarkDataHelper::makeObjectData(bookmark, resource);

        return MultiObjectData{.caption = perObjectData.title,
            .description = bookmark.description,
            .iconPaths = {BookmarkDataHelper::iconPath()},
            .imagePaths = {makeImageRequest(
                resource, bookmark.startTimeMs.count(), kLowImageResolution)},
            .positionMs = bookmark.startTimeMs.count(),
            .durationMs = bookmark.durationMs.count(),
            .count = 1,
            .perObjectData = {std::move(perObjectData)}};
    }

    const auto title = count <= limit ? BookmarkDataHelper::tr("Bookmarks (%1)").arg(count)
                                      : BookmarkDataHelper::tr("Bookmarks (>%1)").arg(limit);

    const auto firstPosition = count <= limit
        ? bookmarks.back().startTimeMs
        : milliseconds(period.startTimeMs); //< If result is cropped, just use period start.

    const auto duration = bookmarks.front().startTimeMs - firstPosition;

    QList<ObjectData> perObjectData;
    if (duration < minimumStackDuration)
    {
        for (const auto& bookmark: nx::utils::reverseRange(bookmarks))
            perObjectData.push_back(BookmarkDataHelper::makeObjectData(bookmark, resource));
    }

    QStringList imagePaths;
    for (const auto& bookmark: bookmarks)
    {
        imagePaths.push_back(
            makeImageRequest(resource, bookmark.startTimeMs.count(), kLowImageResolution));

        if (imagePaths.size() >= kMaxPreviewImageCount)
            break;
    }

    return MultiObjectData{.caption = title,
        .iconPaths = {BookmarkDataHelper::iconPath()},
        .imagePaths = imagePaths,
        .positionMs = firstPosition.count(),
        .durationMs = duration.count(),
        .count = count,
        .perObjectData = perObjectData};
}

} // namespace

struct BookmarkLoaderDelegate::Private
{
    const BackendPtr backend;
    const int maxBookmarksPerBucket;

    // Resources keep a dangling pointer to their system context after it's destroyed, so it's
    // taken from the resource only once, when the delegate is created for that resource.
    const QPointer<common::SystemContext> systemContext;
};

BookmarkLoaderDelegate::BookmarkLoaderDelegate(
    const BackendPtr& backend, int maxBookmarksPerBucket, QObject* parent):
    AbstractLoaderDelegate(parent),
    d(new Private{.backend = backend,
        .maxBookmarksPerBucket = maxBookmarksPerBucket,
        .systemContext =
            backend ? core::SystemContext::fromResource(backend->resource()) : nullptr})
{
    NX_CRITICAL(d->backend);
}

BookmarkLoaderDelegate::~BookmarkLoaderDelegate()
{
    // Required here for forward-declared scoped pointer destruction.
}

QFuture<MultiObjectData> BookmarkLoaderDelegate::load(
    const QnTimePeriod& period, milliseconds minimumStackDuration)
{
    const auto resource = d->backend->resource();
    if (!resource || !d->systemContext || period.isEmpty())
        return {};

    // The merging uses the system context (e.g. the feature access) which is not thread-safe,
    // so it's done in the system context thread. The continuation is canceled if the system
    // context is destroyed.
    // `maxBookmarksPerBucket + 1` limit should be requested to see if there's exactly
    // `maxBookmarksPerBucket` or more bookmarks in the requested `period`.
    return d->backend->load(period, d->maxBookmarksPerBucket + 1)
        .then(d->systemContext.data(),
            [resource, period, minimumStackDuration, limit = d->maxBookmarksPerBucket](
                const QnCameraBookmarkList& bookmarks)
            { return merge(bookmarks, period, minimumStackDuration, limit, resource); });
}

} // namespace timeline
} // namespace nx::vms::client::mobile
