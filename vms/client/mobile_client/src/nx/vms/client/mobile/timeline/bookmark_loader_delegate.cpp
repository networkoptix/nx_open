// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "bookmark_loader_delegate.h"

#include <memory>

#include <QtCore/QFutureWatcher>
#include <QtCore/QPointer>
#include <QtCore/QPromise>

#include <core/resource/camera_bookmark.h>
#include <core/resource/resource.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/mobile/timeline/bookmark_data.h>

namespace nx::vms::client::mobile {
namespace timeline {

using namespace std::chrono;

struct BookmarkLoaderDelegate::Private
{
    const BackendPtr backend;
    const int maxBookmarksPerBucket;

    static void handleLoadingFinished(
        QPromise<MultiObjectData>& promise,
        const QnTimePeriod& period,
        milliseconds minimumStackDuration,
        int limit,
        const QnResourcePtr& resource,
        QnCameraBookmarkList result)
    {
        promise.emplaceResult(
            BookmarkData::merge(result, period, minimumStackDuration, limit, resource));
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
                    d->maxBookmarksPerBucket, resource,
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
