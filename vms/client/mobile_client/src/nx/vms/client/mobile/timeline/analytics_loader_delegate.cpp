// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_loader_delegate.h"

#include <memory>

#include <QtCore/QFutureWatcher>
#include <QtCore/QPointer>
#include <QtCore/QPromise>

#include <analytics/db/analytics_db_types.h>
#include <core/resource/resource.h>
#include <nx/analytics/taxonomy/abstract_state_watcher.h>
#include <nx/analytics/taxonomy/object_type.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/format.h>
#include <nx/utils/range_adapters.h>
#include <nx/vms/client/core/analytics/analytics_icon_manager.h>
#include <nx/vms/client/core/media/chunk_provider.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/text/human_readable.h>

namespace nx::vms::client::mobile {
namespace timeline {

using namespace std::chrono;
using namespace nx::analytics::db;
using namespace nx::analytics::taxonomy;
using nx::vms::text::HumanReadable;

namespace {

constexpr int kMaxTypesPerBucket = 5;

std::shared_ptr<AbstractState> taxonomyState(core::SystemContext* systemContext)
{
    const auto watcher = systemContext->analyticsTaxonomyStateWatcher();
    return NX_ASSERT(watcher) ? watcher->state() : nullptr;
}

ObjectType* objectTypeById(
    const std::shared_ptr<AbstractState>& taxonomy,
    const QString& objectTypeId)
{
    return taxonomy ? taxonomy->objectTypeById(objectTypeId) : nullptr;
}

std::tuple<ObjectType*, QString> trackInfo(
    const std::shared_ptr<AbstractState>& taxonomy,
    const ObjectTrackEx& track)
{
    const auto objectType = objectTypeById(taxonomy, track.objectTypeId);
    auto name = track.title.value_or(Title{}).text;
    if (name.isEmpty())
        name = objectType ? objectType->name() : AnalyticsLoaderDelegate::tr("Unknown Object");

    return {objectType, name};
}

QString iconPath(ObjectType* objectType)
{
    const auto iconManager = core::analytics::IconManager::instance();

    return objectType
        ? iconManager->iconPath(objectType->icon())
        : iconManager->fallbackIconPath();
}

std::tuple<QStringList /*names*/, QStringList /*iconPaths*/> objectTypeInfos(
    const std::shared_ptr<AbstractState>& taxonomy,
    const LookupResult& tracks,
    int limit)
{
    QSet<QString> alreadyAddedIcons;
    QSet<QString> alreadyAddedNames;
    QStringList iconPaths;
    QStringList names;

    for (const auto& track: tracks)
    {
        const auto objectType = objectTypeById(taxonomy, track.objectTypeId);
        const auto path = iconPath(objectType);
        const auto name = objectType->name();

        if (!alreadyAddedNames.contains(name) && names.size() < limit)
        {
            alreadyAddedNames.insert(name);
            names.push_back(name);
        }

        if (!alreadyAddedIcons.contains(path) && iconPaths.size() < limit)
        {
            alreadyAddedIcons.insert(path);
            iconPaths.push_back(path);
        }

        if (names.size() >= limit && iconPaths.size() >= limit)
            break;
    }

    return {names, iconPaths};
}

core::analytics::AttributeList preprocessAttributes(
    core::SystemContext* systemContext,
    const ObjectTrackEx& track)
{
    return systemContext->analyticsAttributeHelper()->
        preprocessAttributes(track.objectTypeId, track.attributes);
}

} // namespace

struct AnalyticsLoaderDelegate::Private
{
    const QPointer<core::ChunkProvider> chunkProvider;
    const BackendPtr backend;
    const int maxTracksPerBucket;

    static QString makeTitleImageRequest(
        const ObjectTrackEx& track, const QString& crossSystemId = {})
    {
        const QString requestTemplate =
            "rest/v4/analytics/objectTracks/%1/titleImage.jpg?deviceId=%2%3";

        const auto systemIdParam =
            crossSystemId.isEmpty() ? "" : nx::format("&%1=%2", systemIdParameter(), crossSystemId);

        return nx::format(requestTemplate,
            track.id.toSimpleString(), track.deviceId.toSimpleString(), systemIdParam);
    }

    static QString makeImageRequest(
        const QnResourcePtr& resource, const ObjectTrackEx& track, int resolution)
    {
        if (!NX_ASSERT(resource && resource->getId() == track.deviceId))
            return {};

        if (track.title.value_or(Title{}).hasImage)
            return makeTitleImageRequest(track, AbstractLoaderDelegate::crossSystemId(resource));

        const milliseconds timestamp = duration_cast<milliseconds>(
            microseconds(track.bestShot.timestampUs));

        return AbstractLoaderDelegate::makeImageRequest(resource, timestamp.count(), resolution,
            {{"objectTrackId", track.id.toSimpleString()}});
    }

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

        const int count = (int) result.size();
        if (count == 1)
        {
            const auto& track = result.front();

            const auto duration = duration_cast<milliseconds>(
                microseconds(track.lastAppearanceTimeUs - track.firstAppearanceTimeUs));

            const auto durationText = HumanReadable::timeSpan(duration,
                HumanReadable::Hours | HumanReadable::Minutes | HumanReadable::Seconds,
                HumanReadable::SuffixFormat::Short, " ");

            const auto [objectType, title] = trackInfo(taxonomyState(systemContext), track);
            const auto position = duration_cast<milliseconds>(microseconds(
                track.firstAppearanceTimeUs));

            ObjectData perObjectData{
                .id = track.id,
                .startTimeMs = position.count(),
                .durationMs = duration.count(),
                .title = title,
                .imagePath = makeImageRequest(resource, track, kHighImageResolution),
                .attributes = preprocessAttributes(systemContext, track)};

            promise.emplaceResult(MultiObjectData{
                .caption = title,
                .description = durationText,
                .iconPaths = {iconPath(objectType)},
                .imagePaths = {makeImageRequest(resource, track, kLowImageResolution)},
                .positionMs = position.count(),
                .durationMs = duration.count(),
                .count = 1,
                .perObjectData = {perObjectData}});
        }
        else if (count > 1)
        {
            const auto title = count <= limit
                ? AnalyticsLoaderDelegate::tr("Objects (%n)", "", count)
                : AnalyticsLoaderDelegate::tr("Objects (>%n)", "", limit);

            const auto firstPosition = count <= limit
                ? duration_cast<milliseconds>(microseconds(result.back().firstAppearanceTimeUs))
                : milliseconds(period.startTimeMs); //< If result is cropped, just use period start.

            const auto duration = duration_cast<milliseconds>(microseconds{
                result.front().firstAppearanceTimeUs}) - firstPosition;

            const auto taxonomy = taxonomyState(systemContext);
            const auto [names, iconPaths] = objectTypeInfos(taxonomy, result, kMaxTypesPerBucket);

            QList<ObjectData> perObjectData;
            if (duration < minimumStackDuration)
            {
                for (const auto& track: nx::utils::reverseRange(result))
                {
                    const auto position = duration_cast<milliseconds>(microseconds(
                        track.firstAppearanceTimeUs));

                    const auto duration = duration_cast<milliseconds>(
                        microseconds(track.lastAppearanceTimeUs - track.firstAppearanceTimeUs));

                    const auto [objectType, title] = trackInfo(taxonomy, track);

                    perObjectData.push_back(ObjectData{
                        .id = track.id,
                        .startTimeMs = position.count(),
                        .durationMs = duration.count(),
                        .title = title,
                        .imagePath = makeImageRequest(resource, track, kHighImageResolution),
                        .attributes = preprocessAttributes(systemContext, track)});
                }
            }

            QStringList imagePaths;
            for (const auto& track: result)
            {
                imagePaths.push_back(makeImageRequest(resource, track, kLowImageResolution));
                if (imagePaths.size() >= kMaxPreviewImageCount)
                    break;
            }

            promise.emplaceResult(MultiObjectData{
                .caption = title,
                .description = names.join(", "),
                .iconPaths = iconPaths,
                .imagePaths = imagePaths,
                .positionMs = firstPosition.count(),
                .durationMs = duration.count(),
                .count = count,
                .perObjectData = perObjectData});
        }
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
