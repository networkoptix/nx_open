// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_data.h"

#include <analytics/db/analytics_db_types.h>
#include <core/resource/resource.h>
#include <nx/analytics/taxonomy/abstract_state_watcher.h>
#include <nx/analytics/taxonomy/object_type.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/format.h>
#include <nx/utils/range_adapters.h>
#include <nx/vms/client/core/analytics/analytics_icon_manager.h>
#include <nx/vms/client/core/bookmarks/bookmark_utils.h>
#include <nx/vms/client/core/qml/qml_ownership.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/thumbnails/remote_async_image_provider.h>
#include <nx/vms/text/human_readable.h>
#include <utils/common/synctime.h>

namespace nx::vms::client::mobile {
namespace timeline {

using namespace std::chrono;
using namespace nx::analytics::db;
using namespace nx::analytics::taxonomy;
using nx::vms::text::HumanReadable;

namespace {

constexpr int kMaxTypesPerBucket = 5;

// If the last object detection timestamp is more than this interval old (compared to the current
// sync time), the track is considered finished and its preview thumbnail is not reloaded by timer.
// The server uses adjustable timeout which is 3 minutes by default.
constexpr seconds kTrackTerminationThreshold = 5min;

constexpr seconds kThumbnailExpirationInterval = 30s; //< For ongoing tracks only.

ObjectType* objectTypeById(
    const std::shared_ptr<AbstractState>& taxonomy,
    const std::string& objectTypeId)
{
    return taxonomy ? taxonomy->objectTypeById(objectTypeId) : nullptr;
}

QString iconPath(ObjectType* objectType)
{
    const auto iconManager = core::analytics::IconManager::instance();

    return objectType
        ? iconManager->iconPath(QString::fromStdString(objectType->icon()))
        : iconManager->fallbackIconPath();
}

std::tuple<QStringList /*names*/, QStringList /*iconPaths*/> objectTypeInfos(
    const std::shared_ptr<AbstractState>& taxonomy,
    std::span<const analytics::db::ObjectTrackEx> tracks,
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
        const auto name = objectType ? QString::fromStdString(objectType->name()) : "";

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
        preprocessAttributes(QString::fromStdString(track.objectTypeId), track.attributes);
}

std::optional<seconds> thumbnailExpirationInterval(const ObjectTrackEx& track)
{
    const auto timeSinceLastDetection =
        qnSyncTime->currentTimePoint() - microseconds(track.lastAppearanceTimeUs);

    const bool isTrackTerminated = timeSinceLastDetection > kTrackTerminationThreshold;
    return isTrackTerminated ? std::nullopt : std::make_optional(kThumbnailExpirationInterval);
}

static QString makeTitleImageRequest(const ObjectTrackEx& track,
    std::optional<seconds> expiration = std::nullopt, const QString& crossSystemId = {})
{
    static const QString kRequestTemplate =
        "rest/v4/analytics/objectTracks/%1/titleImage.jpg?deviceId=%2%3%4";

    static const QString kExtraParamTemplate = "&%1=%2";

    const auto systemIdParam = crossSystemId.isEmpty()
        ? ""
        : nx::format(kExtraParamTemplate, AbstractObjectData::systemIdParameterName(), crossSystemId);

    const auto expirationParam = expiration
        ? nx::format(kExtraParamTemplate,
            core::RemoteAsyncImageProvider::expirationParameterName(), expiration->count())
        : "";

    return nx::format(kRequestTemplate, track.id.toSimpleString(),
        track.deviceId.toSimpleString(), expirationParam, systemIdParam);
}

static QString makeAnalyticsImageRequest(
    const QnResourcePtr& resource, const ObjectTrackEx& track, int resolution)
{
    if (!NX_ASSERT(resource && resource->getId() == track.deviceId))
        return {};

    if (track.title.value_or(Title{}).hasImage)
    {
        return makeTitleImageRequest(track, thumbnailExpirationInterval(track),
            AbstractObjectData::crossSystemId(resource));
    }

    QList<std::pair<QString, QString>> extraParameters{
        {"objectTrackId", track.id.toSimpleString()}};

    if (const auto expiration = thumbnailExpirationInterval(track))
    {
        extraParameters.emplaceBack(core::RemoteAsyncImageProvider::expirationParameterName(),
            QString::number(expiration->count()));
    }

    const milliseconds timestamp = duration_cast<milliseconds>(
        microseconds(track.bestShot.timestampUs));

    return AbstractObjectData::makeImageRequest(resource, timestamp.count(), resolution,
        extraParameters);
}

} // namespace

AnalyticsData::AnalyticsData(
    const analytics::db::ObjectTrack& track,
    const QnResourcePtr& resource)
    :
    m_resource(resource)
{
    update(track);
}

nx::Uuid AnalyticsData::id() const
{
    return m_track.id;
}

qint64 AnalyticsData::startTimeMs() const
{
    return duration_cast<milliseconds>(microseconds(m_track.firstAppearanceTimeUs)).count();
}

qint64 AnalyticsData::durationMs() const
{
    return duration_cast<milliseconds>(microseconds(
        m_track.lastAppearanceTimeUs - m_track.firstAppearanceTimeUs)).count();
}

QString AnalyticsData::title() const
{
    return m_title;
}

QString AnalyticsData::description() const
{
    return {};
}

QString AnalyticsData::imagePath() const
{
    return m_imagePath;
}

QVariant AnalyticsData::tags() const
{
    return QStringList{};
}

QVariant AnalyticsData::attributes() const
{
    return QVariant::fromValue(m_attributes);
}

QnResourcePtr AnalyticsData::resource() const
{
    return m_resource;
}

void AnalyticsData::update(analytics::db::ObjectTrack track)
{
    m_track = std::move(track);
    m_imagePath = makeAnalyticsImageRequest(m_resource, m_track, kHighImageResolution);

    const auto systemContext = m_resource->systemContext()
        ? m_resource->systemContext()->as<core::SystemContext>()
        : nullptr;

    if (!NX_ASSERT(systemContext))
        return;

    m_attributes = preprocessAttributes(systemContext, m_track);
    m_title = core::bookmarks::bookmarkNameFromObjectTrack(m_track, systemContext);

    emit changed();
}

MultiObjectData AnalyticsData::merge(
    std::span<const analytics::db::ObjectTrackEx> tracks,
    const QnTimePeriod& period,
    std::chrono::milliseconds minimumStackDuration,
    int limit,
    const QnResourcePtr& resource)
{
    const auto systemContext =
        resource->systemContext() ? resource->systemContext()->as<core::SystemContext>() : nullptr;

    if (!NX_ASSERT(systemContext))
        return {};

    const int count = (int) tracks.size();
    if (count == 1)
    {
        const auto& track = tracks.front();

        const auto objectType =
            objectTypeById(systemContext->analyticsTaxonomyState(), track.objectTypeId);

        const auto perObjectData = std::make_shared<AnalyticsData>(track, resource);

        const auto durationText = HumanReadable::timeSpan(
            milliseconds{perObjectData->durationMs()},
            HumanReadable::Hours | HumanReadable::Minutes | HumanReadable::Seconds,
            HumanReadable::SuffixFormat::Short, " ");

        return MultiObjectData{
            .caption = perObjectData->title(),
            .description = durationText,
            .iconPaths = {iconPath(objectType)},
            .imagePaths = {makeAnalyticsImageRequest(resource, track, kLowImageResolution)},
            .positionMs = perObjectData->startTimeMs(),
            .durationMs = perObjectData->durationMs(),
            .count = 1,
            .perObjectData = {perObjectData}};
    }
    else if (count > 1)
    {
        const auto title = count <= limit
            ? AnalyticsData::tr("Objects (%1)").arg(count)
            : AnalyticsData::tr("Objects (>%1)").arg(limit);

        const auto firstPosition = count <= limit
            ? duration_cast<milliseconds>(microseconds(tracks.back().firstAppearanceTimeUs))
            : milliseconds(period.startTimeMs); //< If result is cropped, just use period start.

        const auto duration = duration_cast<milliseconds>(microseconds{
            tracks.front().firstAppearanceTimeUs}) - firstPosition;

        const auto taxonomy = systemContext->analyticsTaxonomyState();
        const auto [names, iconPaths] = objectTypeInfos(taxonomy, tracks, kMaxTypesPerBucket);

        QList<std::shared_ptr<AbstractObjectData>> perObjectData;
        if (duration < minimumStackDuration)
        {
            for (const auto& track: nx::utils::reverseRange(tracks))
            {
                perObjectData.push_back(std::make_shared<AnalyticsData>(track, resource));
            }
        }

        QStringList imagePaths;
        for (const auto& track: tracks)
        {
            imagePaths.push_back(makeAnalyticsImageRequest(resource, track, kLowImageResolution));
            if (imagePaths.size() >= kMaxPreviewImageCount)
                break;
        }

        return MultiObjectData{
            .caption = title,
            .description = names.join(", "),
            .iconPaths = iconPaths,
            .imagePaths = imagePaths,
            .positionMs = firstPosition.count(),
            .durationMs = duration.count(),
            .count = count,
            .perObjectData = perObjectData};
    }

    return {};
}

} // namespace timeline
} // namespace nx::vms::client::mobile
