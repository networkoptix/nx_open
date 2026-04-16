// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_loader_delegate.h"

#include <memory>

#include <QtCore/QFutureWatcher>
#include <QtCore/QPointer>
#include <QtCore/QPromise>

#include <core/resource/resource.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/media/chunk_provider.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/mobile/timeline/analytics_data.h>

namespace nx::vms::client::mobile {
namespace timeline {

using namespace std::chrono;
using namespace nx::analytics::db;

struct AnalyticsLoaderDelegate::Private
{
    const QPointer<core::ChunkProvider> chunkProvider;
    const BackendPtr backend;
    const int maxTracksPerBucket;

    static void handleLoadingFinished(
        const QnResourcePtr& resource,
        QPromise<MultiObjectData>& promise,
        const QnTimePeriod& period,
        milliseconds minimumStackDuration,
        int limit,
        LookupResult&& result)
    {
        const auto systemContext = resource->systemContext()->as<core::SystemContext>();
        if (!NX_ASSERT(systemContext))
            return;

        promise.emplaceResult(
            AnalyticsData::merge(result, period, minimumStackDuration, limit, resource));
    }
};

AnalyticsLoaderDelegate::AnalyticsLoaderDelegate(
    core::ChunkProvider* chunkProvider,
    const BackendPtr& backend,
    int maxTracksPerBucket,
    QObject* parent)
    :
    AbstractLoaderDelegate(parent),
    d(new Private{
        .chunkProvider = chunkProvider,
        .backend = backend,
        .maxTracksPerBucket = maxTracksPerBucket})
{
    NX_ASSERT(d->backend);
}

AnalyticsLoaderDelegate::~AnalyticsLoaderDelegate()
{
    // Required here for forward-declared scoped pointer destruction.
}

QFuture<MultiObjectData> AnalyticsLoaderDelegate::load(
    const QnTimePeriod& period, milliseconds minimumStackDuration)
{
    if (!d->chunkProvider || !d->backend || period.isEmpty())
        return {};

    // `maxTracksPerBucket + 1` limit should be requested to see if there's exactly
    // `maxTracksPerBucket` or more tracks in the requested `period`.
    const auto nativeDataFuture = d->backend->load(period, d->maxTracksPerBucket + 1);
    if (nativeDataFuture.isCanceled())
        return {};

    using Watcher = QFutureWatcher<LookupResult>;
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
                Private::handleLoadingFinished(resource, resultPromise, period,
                    minimumStackDuration, d->maxTracksPerBucket,
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
