// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "bookmark_utils.h"

#include <chrono>

#include <QtCore/QCoreApplication>

#include <core/resource/camera_history.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_management/resource_pool.h>
#include <nx/analytics/taxonomy/abstract_state_watcher.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/socket_global.h>
#include <nx/network/url/url_builder.h>
#include <nx/vms/client/core/analytics/analytics_attribute_helper.h>
#include <nx/vms/client/core/bookmarks/bookmark_constants.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/watchers/feature_access_watcher.h>
#include <nx/vms/client/core/watchers/server_time_watcher.h>
#include <nx/vms/client/core/watchers/user_watcher.h>
#include <nx/vms/common/api/helpers/bookmark_api_converter.h>
#include <nx/vms/common/bookmark/bookmark_facade.h>
#include <nx/vms/common/saas/saas_service_manager.h>
#include <nx/vms/common/saas/saas_utils.h>
#include <utils/common/synctime.h>

namespace nx::vms::client::core {
namespace bookmarks {

struct BookmarkUtilsStrings
{
    Q_DECLARE_TR_FUNCTIONS(nx::vms::client::core::bookmarks::BookmarkUtilsStrings)
};

QString getVisibleBookmarkCreatorName(
    const QnCameraBookmark& bookmark,
    SystemContext* systemContext)
{
    const auto currentUser = systemContext->userWatcher()->user();

    if (!NX_ASSERT(currentUser))
        return {};

    if (bookmark.creatorId == QnCameraBookmark::systemUserId()
        || systemContext->resourceAccessManager()->hasPowerUserPermissions(currentUser)
        || currentUser->getId() == bookmark.creatorId)
    {
        return common::QnBookmarkFacade::creatorName(bookmark, systemContext->resourcePool());
    }

    return {}; //< Only power users can see name of other users.
}

QnMediaServerResourcePtr getServerForBookmark(
    const common::CameraBookmark& bookmark,
    SystemContext* context)
{
    if (!NX_ASSERT(context))
        return {};

    const auto camera =
        context->resourcePool()->getResourceById<QnVirtualCameraResource>(bookmark.cameraId);

    if (!camera)
        return {};

    return context->cameraHistoryPool()->getMediaServerOnTime(
        camera, bookmark.startTimeMs.count());
}

std::optional<Url> getBookmarkSharingLink(
    const common::CameraBookmark& bookmark,
    SystemContext* context)
{
    if (!NX_ASSERT(context))
        return {};

    const auto server = getServerForBookmark(bookmark, context);
    if (!server)
        return {};

    const auto cloudSystemId = context->cloudSystemId();
    if (cloudSystemId.isEmpty())
        return {};

    const auto apiBookmark = common::bookmarkToApi<api::BookmarkV3>(bookmark, server->getId());

    return nx::network::url::Builder()
        .setScheme(network::http::kSecureUrlSchemeName)
        .setHost(nx::network::SocketGlobals::cloud().cloudHost())
        .setPath(QString("share/%1/%2").arg(cloudSystemId, apiBookmark.id))
            .toUrl();
}

bool bookmarkSharingAvailable(SystemContext* context)
{
    if (!NX_ASSERT(context && context->mode() == SystemContext::Mode::client))
        return false;

    if (!nx::vms::common::saas::saasInitialized(context))
        return false;

    if (context->saasServiceManager()->saasSuspendedOrShutDown())
        return false;

    const auto featureAccess = context->featureAccess();
    if (!NX_ASSERT(featureAccess))
        return false;

    return featureAccess->canUseShareBookmark();
}

common::BookmarkShareableParams defaultBookmarkSharingParams()
{
    common::BookmarkShareableParams result;
    result.shareable = true;
    result.expirationTimeMs = qnSyncTime->value() + std::chrono::days(1);

    return result;
}

bool sharedBookmarkAccessible(const common::CameraBookmark& bookmark, SystemContext* context)
{
    return bookmarkSharingAvailable(context)
        && bookmark.shareable()
        && bookmark.bookmarkMatchesFilter(api::BookmarkShareFilter::shared
            | api::BookmarkShareFilter::accessible);
}

std::optional<common::CameraBookmark> bookmarkFromObjectTrack(
    const nx::analytics::db::ObjectTrack& track,
    const QnVirtualCameraResourcePtr& camera)
{
    using namespace std::chrono;
    using namespace std::chrono_literals;

    static constexpr auto kMinBookmarkDuration = 8000ms;

    if (!NX_ASSERT(camera))
        return {};

    const auto systemContext = camera->systemContext()
        ? camera->systemContext()->as<core::SystemContext>()
        : nullptr;

    if (!systemContext)
        return {};

    const auto startTime = duration_cast<milliseconds>(microseconds(track.firstAppearanceTimeUs));
    const auto duration = duration_cast<milliseconds>(microseconds(
        track.lastAppearanceTimeUs - track.firstAppearanceTimeUs));

    const auto attributes = systemContext->analyticsAttributeHelper()->
        preprocessAttributes(QString::fromStdString(track.objectTypeId), track.attributes);

    nx::vms::common::CameraBookmark result;

    result.guid = nx::Uuid::createUuid();
    result.cameraId = camera->getId();
    result.startTimeMs = startTime;
    result.durationMs = std::max(duration, kMinBookmarkDuration);
    result.tags = { BookmarkConstants::objectBasedTagName() };
    result.name = bookmarkNameFromObjectTrack(track, systemContext);
    result.description = bookmarkDescription(startTime, camera, attributes);

    return result;
}

QString bookmarkNameFromObjectTrack(
    const nx::analytics::db::ObjectTrack& track,
    SystemContext* systemContext)
{
    if (!NX_ASSERT(systemContext))
        return {};

    if (track.title.has_value() && !track.title->text.isEmpty())
        return track.title->text;

    const auto taxonomyState = systemContext->analyticsTaxonomyState();
    const auto objectType = taxonomyState
        ? taxonomyState->objectTypeById(track.objectTypeId)
        : nullptr;

    return objectType
        ? QString::fromStdString(objectType->name())
        : BookmarkUtilsStrings::tr("Unknown Object");
}

QString bookmarkDescription(
    std::chrono::milliseconds startTime,
    const QnMediaResourcePtr& camera,
    const analytics::AttributeList& attributes)
{
    if (!NX_ASSERT(camera))
        return {};

    const auto dateTime = QDateTime::fromMSecsSinceEpoch(
        startTime.count(), nx::vms::client::core::ServerTimeWatcher::timeZone(camera));

    static const QString kLineTemplate("%1: %2");

    QStringList resultLines;
    resultLines.append(dateTime.toString());
    resultLines.append(kLineTemplate.arg(BookmarkUtilsStrings::tr("Camera"), camera->getName()));

    for (const auto& attribute: attributes)
    {
        resultLines.append(kLineTemplate.arg(
            attribute.displayedName,
            attribute.displayedValues.join(", ")));
    }

    return resultLines.join("\n");
}

} // namespace bookmarks
} // namespace nx::vms::client::core
