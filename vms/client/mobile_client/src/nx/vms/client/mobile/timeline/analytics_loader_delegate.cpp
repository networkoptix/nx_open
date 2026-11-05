// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_loader_delegate.h"

#include <QtCore/QPointer>

#include <core/resource/resource.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/range_adapters.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/mobile/timeline/analytics_data_helper.h>

namespace nx::vms::client::mobile {
namespace timeline {

using namespace std::chrono;
using namespace nx::analytics::db;

namespace {

constexpr int kMaxTypesPerBucket = 5;

MultiObjectData merge(std::span<const ObjectTrackEx> tracks,
    const QnTimePeriod& period,
    milliseconds minimumStackDuration,
    int limit,
    const QnResourcePtr& resource)
{
    if (tracks.empty())
        return {};

    const auto systemContext = core::SystemContext::fromResource(resource);
    if (!NX_ASSERT(systemContext))
        return {};

    const int count = (int) tracks.size();
    if (count == 1)
    {
        const auto& track = tracks.front();
        auto perObjectData = AnalyticsDataHelper::makeObjectData(track, resource);

        return MultiObjectData{.caption = perObjectData.title,
            .description = perObjectData.description,
            .iconPaths = {perObjectData.iconPath},
            .imagePaths = {AnalyticsDataHelper::makeImageRequest(
                resource, track, kLowImageResolution)},
            .positionMs = perObjectData.startTimeMs,
            .durationMs = perObjectData.durationMs,
            .count = 1,
            .perObjectData = {std::move(perObjectData)}};
    }

    const auto title = count <= limit ? AnalyticsDataHelper::tr("Objects (%1)").arg(count)
                                      : AnalyticsDataHelper::tr("Objects (>%1)").arg(limit);

    const auto firstPosition = count <= limit
        ? duration_cast<milliseconds>(microseconds(tracks.back().firstAppearanceTimeUs))
        : milliseconds(period.startTimeMs); //< If result is cropped, just use period start.

    const auto duration =
        duration_cast<milliseconds>(microseconds{tracks.front().firstAppearanceTimeUs})
        - firstPosition;

    const auto [names, iconPaths] = AnalyticsDataHelper::objectTypeInfos(
        systemContext->analyticsTaxonomyState(), tracks, kMaxTypesPerBucket);

    QList<ObjectData> perObjectData;
    if (duration < minimumStackDuration)
    {
        for (const auto& track: nx::utils::reverseRange(tracks))
            perObjectData.push_back(AnalyticsDataHelper::makeObjectData(track, resource));
    }

    QStringList imagePaths;
    for (const auto& track: tracks)
    {
        imagePaths.push_back(
            AnalyticsDataHelper::makeImageRequest(resource, track, kLowImageResolution));

        if (imagePaths.size() >= kMaxPreviewImageCount)
            break;
    }

    return MultiObjectData{.caption = title,
        .description = names.join(", "),
        .iconPaths = iconPaths,
        .imagePaths = imagePaths,
        .positionMs = firstPosition.count(),
        .durationMs = duration.count(),
        .count = count,
        .perObjectData = perObjectData};
}

} // namespace

struct AnalyticsLoaderDelegate::Private
{
    const BackendPtr backend;
    const int maxTracksPerBucket;

    // Resources keep a dangling pointer to their system context after it's destroyed, so it's
    // taken from the resource only once, when the delegate is created for that resource.
    const QPointer<common::SystemContext> systemContext;
};

AnalyticsLoaderDelegate::AnalyticsLoaderDelegate(
    const BackendPtr& backend, int maxTracksPerBucket, QObject* parent):
    AbstractLoaderDelegate(parent),
    d(new Private{.backend = backend,
        .maxTracksPerBucket = maxTracksPerBucket,
        .systemContext =
            backend ? core::SystemContext::fromResource(backend->resource()) : nullptr})
{
    NX_CRITICAL(d->backend);
}

AnalyticsLoaderDelegate::~AnalyticsLoaderDelegate()
{
    // Required here for forward-declared scoped pointer destruction.
}

QFuture<MultiObjectData> AnalyticsLoaderDelegate::load(
    const QnTimePeriod& period, milliseconds minimumStackDuration)
{
    const auto resource = d->backend->resource();
    if (!resource || !d->systemContext || period.isEmpty())
        return {};

    // The merging uses the system context (e.g. the analytics taxonomy) which is not thread-safe,
    // so it's done in the system context thread. The continuation is canceled if the system
    // context is destroyed.
    // `maxTracksPerBucket + 1` limit should be requested to see if there's exactly
    // `maxTracksPerBucket` or more tracks in the requested `period`.
    return d->backend->load(period, d->maxTracksPerBucket + 1)
        .then(d->systemContext.data(),
            [resource, period, minimumStackDuration, limit = d->maxTracksPerBucket](
                const LookupResult& tracks)
            { return merge(tracks, period, minimumStackDuration, limit, resource); });
}

} // namespace timeline
} // namespace nx::vms::client::mobile
