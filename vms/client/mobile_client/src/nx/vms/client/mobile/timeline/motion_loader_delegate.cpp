// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "motion_loader_delegate.h"

#include <QtCore/QPointer>
#include <QtCore/QPromise>

#include <core/resource/resource.h>
#include <nx/vms/client/core/media/chunk_provider.h>
#include <nx/vms/client/mobile/timeline/motion_data.h>

namespace nx::vms::client::mobile {
namespace timeline {

using namespace std::chrono;

struct MotionLoaderDelegate::Private
{
    const QPointer<core::ChunkProvider> chunkProvider;
    const int maxMotionsPerBucket;
};

MotionLoaderDelegate::MotionLoaderDelegate(
    core::ChunkProvider* chunkProvider,
    int maxMotionsPerBucket,
    QObject* parent)
    :
    AbstractLoaderDelegate(parent),
    d(new Private{.chunkProvider = chunkProvider, .maxMotionsPerBucket = maxMotionsPerBucket})
{
}

MotionLoaderDelegate::~MotionLoaderDelegate()
{
    // Required here for forward-declared scoped pointer destruction.
}

QFuture<MultiObjectData> MotionLoaderDelegate::load(
    const QnTimePeriod& period, milliseconds /*minimumStackDuration*/)
{
    if (!d->chunkProvider || period.isEmpty())
        return {};

    const auto resource = d->chunkProvider->resource();
    if (!NX_ASSERT(resource))
        return {};

    const auto chunks = d->chunkProvider->periods(Qn::MotionContent);
    if (chunks.empty())
        return {};

    const auto begin = std::lower_bound(chunks.cbegin(), chunks.cend(), period.startTimeMs);
    const auto end = std::upper_bound(chunks.cbegin(), chunks.cend(), period.endTimeMs() - 1);

    const int count = (int) std::distance(begin, end);
    if (count == 0)
        return {};

    QPromise<MultiObjectData> promise;
    promise.start();

    promise.emplaceResult(MotionData::merge({begin, end}, d->maxMotionsPerBucket, resource));

    promise.finish();
    return promise.future();
}

} // namespace timeline
} // namespace nx::vms::client::mobile
