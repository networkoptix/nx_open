// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "motion_loader_delegate.h"

#include <span>

#include <QtCore/QPointer>

#include <core/resource/resource.h>
#include <nx/utils/range_adapters.h>
#include <nx/vms/client/core/media/abstract_time_period_storage.h>
#include <nx/vms/client/mobile/timeline/motion_data_helper.h>
#include <recording/time_period_list.h>

namespace nx::vms::client::mobile {
namespace timeline {

using namespace std::chrono;

namespace {

MultiObjectData merge(
    std::span<const QnTimePeriod> motions, int maxMotionsPerBucket, const QnResourcePtr& resource)
{
    if (motions.empty())
        return {};

    if (motions.size() == 1)
    {
        const auto& motion = motions.front();
        auto perObjectData = MotionDataHelper::makeObjectData(motion, resource);

        return MultiObjectData{.caption = perObjectData.title,
            .description = perObjectData.description,
            .iconPaths = {MotionDataHelper::iconPath()},
            .imagePaths = {makeImageRequest(
                resource, MotionDataHelper::previewTimeMs(motion), kLowImageResolution)},
            .positionMs = motion.startTimeMs,
            .durationMs = motion.durationMs,
            .count = 1,
            .perObjectData = {std::move(perObjectData)}};
    }

    QStringList imagePaths;
    for (const auto& motion: nx::utils::reverseRange(motions))
    {
        imagePaths.push_back(makeImageRequest(
            resource, MotionDataHelper::previewTimeMs(motion), kLowImageResolution));

        if (imagePaths.size() >= kMaxPreviewImageCount)
            break;
    }

    return MultiObjectData{.caption = (int) motions.size() > maxMotionsPerBucket
            ? MotionDataHelper::tr("Motion (>%1)").arg(maxMotionsPerBucket)
            : MotionDataHelper::tr("Motion (%1)").arg(motions.size()),
        .iconPaths = {MotionDataHelper::iconPath()},
        .imagePaths = imagePaths,
        .positionMs = motions.front().startTimeMs,
        .durationMs = motions.back().startTimeMs - motions.front().startTimeMs,
        .count = (int) motions.size()};
}

} // namespace

struct MotionLoaderDelegate::Private
{
    const QPointer<core::AbstractTimePeriodStorage> periodStorage;
    const QnResourcePtr resource;
    const int maxMotionsPerBucket;
};

MotionLoaderDelegate::MotionLoaderDelegate(core::AbstractTimePeriodStorage* periodStorage,
    const QnResourcePtr& resource,
    int maxMotionsPerBucket,
    QObject* parent):
    AbstractLoaderDelegate(parent),
    d(new Private{.periodStorage = periodStorage,
        .resource = resource,
        .maxMotionsPerBucket = maxMotionsPerBucket})
{
}

MotionLoaderDelegate::~MotionLoaderDelegate()
{
    // Required here for forward-declared scoped pointer destruction.
}

QFuture<MultiObjectData> MotionLoaderDelegate::load(
    const QnTimePeriod& period, milliseconds /*minimumStackDuration*/)
{
    if (!d->periodStorage || !d->resource || period.isEmpty())
        return {};

    const auto& chunks = d->periodStorage->periods(Qn::MotionContent);
    if (chunks.empty())
        return {};

    const auto begin = std::lower_bound(chunks.cbegin(), chunks.cend(), period.startTimeMs);
    const auto end = std::upper_bound(chunks.cbegin(), chunks.cend(), period.endTimeMs() - 1);

    const int count = (int) std::distance(begin, end);
    if (count == 0)
        return {};

    return QtFuture::makeReadyValueFuture(
        merge({begin, end}, d->maxMotionsPerBucket, d->resource));
}

} // namespace timeline
} // namespace nx::vms::client::mobile
