// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "motion_loader_delegate.h"

#include <QtCore/QPointer>
#include <QtCore/QPromise>

#include <core/resource/resource.h>
#include <nx/vms/client/core/media/chunk_provider.h>
#include <nx/vms/text/human_readable.h>

namespace nx::vms::client::mobile {
namespace timeline {

using namespace std::chrono;
using nx::vms::text::HumanReadable;

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

    const auto last = end - 1;

    const auto getPreviewTimeMs =
        [](const QnTimePeriod& motion) { return motion.startTimeMs + motion.durationMs / 2; };

    if (count == 1)
    {
        const auto durationText = HumanReadable::timeSpan(begin->duration(),
            HumanReadable::Hours | HumanReadable::Minutes | HumanReadable::Seconds,
            HumanReadable::SuffixFormat::Short, " ");

        const auto previewTimeMs = getPreviewTimeMs(*last);

        ObjectData perObjectData{
            .startTimeMs = begin->startTimeMs,
            .durationMs = begin->durationMs,
            .title = tr("Motion detected"),
            .imagePath = makeImageRequest(resource, previewTimeMs, kHighImageResolution)};

        promise.emplaceResult(MultiObjectData{
            .caption = tr("Motion detected"),
            .description = durationText,
            .iconPaths = {"image://skin/20x20/Outline/motion.svg"},
            .imagePaths = {makeImageRequest(resource, previewTimeMs, kLowImageResolution)},
            .positionMs = begin->startTimeMs,
            .durationMs = begin->durationMs,
            .count = 1,
            .perObjectData = {perObjectData}});
    }
    else
    {
        QStringList imagePaths;
        for (auto it = last;; --it)
        {
            imagePaths.push_back(
                makeImageRequest(resource, getPreviewTimeMs(*it), kLowImageResolution));
            if (it == begin || imagePaths.size() >= kMaxPreviewImageCount)
                break;
        }

        promise.emplaceResult(MultiObjectData{
            .caption = count > d->maxMotionsPerBucket
                ? tr("Motions (>%n)", "", d->maxMotionsPerBucket)
                : tr("Motions (%n)", "", count),
            .iconPaths = {"image://skin/20x20/Outline/motion.svg"},
            .imagePaths = imagePaths,
            .positionMs = begin->startTimeMs,
            .durationMs = last->startTimeMs - begin->startTimeMs,
            .count = count});
    }

    promise.finish();
    return promise.future();
}

} // namespace timeline
} // namespace nx::vms::client::mobile
