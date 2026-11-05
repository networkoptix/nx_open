// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_data_helper.h"

#include <analytics/db/analytics_db_types.h>
#include <core/resource/resource.h>
#include <nx/analytics/taxonomy/abstract_state_watcher.h>
#include <nx/analytics/taxonomy/object_type.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/format.h>
#include <nx/vms/client/core/analytics/analytics_icon_manager.h>
#include <nx/vms/client/core/bookmarks/bookmark_utils.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/thumbnails/remote_async_image_provider.h>
#include <nx/vms/text/human_readable.h>
#include <utils/common/synctime.h>

namespace nx::vms::client::mobile {
namespace timeline {

using namespace std::chrono;
using namespace nx::analytics::db;
using namespace nx::analytics::taxonomy;
using namespace Qt::StringLiterals;

using nx::vms::text::HumanReadable;

namespace {

// If the last object detection timestamp is more than this interval old (compared to the current
// sync time), the track is considered finished and its preview thumbnail is not reloaded by timer.
// The server uses adjustable timeout which is 3 minutes by default.
constexpr seconds kTrackTerminationThreshold = 5min;

constexpr seconds kThumbnailExpirationInterval = 30s; //< For ongoing tracks only.

ObjectType* objectTypeById(
    const std::shared_ptr<AbstractState>& taxonomy, const std::string& objectTypeId)
{
    return taxonomy ? taxonomy->objectTypeById(objectTypeId) : nullptr;
}

QString getIconPath(ObjectType* objectType)
{
    const auto iconManager = core::analytics::IconManager::instance();

    if (!objectType)
        return iconManager->fallbackIconPath();

    return iconManager->iconPath(QString::fromStdString(objectType->icon()));
}

std::optional<seconds> thumbnailExpirationInterval(const ObjectTrack& track)
{
    const auto timeSinceLastDetection =
        qnSyncTime->currentTimePoint() - microseconds(track.lastAppearanceTimeUs);

    const bool isTrackTerminated = timeSinceLastDetection > kTrackTerminationThreshold;
    return isTrackTerminated ? std::nullopt : std::make_optional(kThumbnailExpirationInterval);
}

static QString makeTitleImageRequest(const ObjectTrack& track,
    std::optional<seconds> expiration = std::nullopt,
    const QString& crossSystemId = {})
{
    static const QString kRequestTemplate =
        "rest/v4/analytics/objectTracks/%1/titleImage.jpg?deviceId=%2%3%4";

    static const QString kExtraParamTemplate = "&%1=%2";

    const auto systemIdParam = crossSystemId.isEmpty()
        ? ""
        : nx::format(kExtraParamTemplate, systemIdParameterName(), crossSystemId);

    const auto expirationParamName = core::RemoteAsyncImageProvider::expirationParameterName();
    const auto expirationParam = expiration
        ? nx::format(kExtraParamTemplate, expirationParamName, expiration->count())
        : "";

    return nx::format(kRequestTemplate,
        track.id.toSimpleString(),
        track.deviceId.toSimpleString(),
        expirationParam,
        systemIdParam);
}

} // namespace

ObjectData AnalyticsDataHelper::makeObjectData(
    const ObjectTrack& track, const QnResourcePtr& resource)
{
    const auto startTime = duration_cast<milliseconds>(microseconds(track.firstAppearanceTimeUs));
    const auto duration = duration_cast<milliseconds>(
        microseconds(track.lastAppearanceTimeUs - track.firstAppearanceTimeUs));

    ObjectData result{.id = track.id,
        .startTimeMs = startTime.count(),
        .durationMs = duration.count(),
        .description = HumanReadable::timeSpan(duration,
            HumanReadable::Hours | HumanReadable::Minutes | HumanReadable::Seconds,
            HumanReadable::SuffixFormat::Short,
            u" "_s),
        .imagePath = makeImageRequest(resource, track, kHighImageResolution),
        .source = QVariant::fromValue(track)};

    const auto systemContext = core::SystemContext::fromResource(resource);

    if (!NX_ASSERT(systemContext))
        return result;

    result.attributes = systemContext->analyticsAttributeHelper()->preprocessAttributes(
        QString::fromStdString(track.objectTypeId), track.attributes);
    result.title = core::bookmarks::bookmarkNameFromObjectTrack(track, systemContext);
    result.iconPath =
        getIconPath(objectTypeById(systemContext->analyticsTaxonomyState(), track.objectTypeId));

    return result;
}

QString AnalyticsDataHelper::makeImageRequest(
    const QnResourcePtr& resource, const ObjectTrack& track, int resolution)
{
    if (!NX_ASSERT(resource && resource->getId() == track.deviceId))
        return {};

    if (track.title.value_or(Title{}).hasImage)
    {
        return makeTitleImageRequest(
            track, thumbnailExpirationInterval(track), crossSystemId(resource));
    }

    QList<std::pair<QString, QString>> extraParameters{
        {u"objectTrackId"_s, track.id.toSimpleString()}};

    if (const auto expiration = thumbnailExpirationInterval(track))
    {
        extraParameters.emplaceBack(core::RemoteAsyncImageProvider::expirationParameterName(),
            QString::number(expiration->count()));
    }

    const milliseconds timestamp =
        duration_cast<milliseconds>(microseconds(track.bestShot.timestampUs));

    return timeline::makeImageRequest(resource, timestamp.count(), resolution, extraParameters);
}

std::tuple<QStringList /*names*/, QStringList /*iconPaths*/> AnalyticsDataHelper::objectTypeInfos(
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
        const auto path = getIconPath(objectType);
        const auto name = objectType ? QString::fromStdString(objectType->name()) : tr("Unknown");

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

} // namespace timeline
} // namespace nx::vms::client::mobile
