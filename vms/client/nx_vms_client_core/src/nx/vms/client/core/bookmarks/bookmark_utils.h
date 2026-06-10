// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <analytics/db/analytics_db_types.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource/resource_fwd.h>
#include <nx/utils/url.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/analytics/analytics_attribute_helper.h>

namespace nx::vms::client::core {

class SystemContext;

namespace bookmarks {

QString NX_VMS_CLIENT_CORE_API getVisibleBookmarkCreatorName(
    const common::CameraBookmark& bookmark,
    SystemContext* context);

/**
 * @param context Bookmark camera system context is expected.
 */
QnMediaServerResourcePtr NX_VMS_CLIENT_CORE_API getServerForBookmark(
    const common::CameraBookmark& bookmark,
    SystemContext* context);

/**
 * Bookmark sharing feature precondition check.
 * @param context Main system context is expected.
 * @return True if bookmark sharing feature is available for the site and its current state allows
 *     using it.
 */
bool NX_VMS_CLIENT_CORE_API bookmarkSharingAvailable(SystemContext* context);

/**
 * @return Sharing parameters for non-interactive share actions: 1 day expiration time, no password
 *     protection.
 */
common::BookmarkShareableParams NX_VMS_CLIENT_CORE_API defaultBookmarkSharingParams();

/**
 * Builds the public cloud share URL for the given bookmark. Performs no feature availability,
 * bookmark sharing state, or shared bookmark expiration checks - the returned link is
 * not guaranteed to resolve to an accessible share.
 * @return Url if passed parameters are minimum sufficient to yeld link, nullopt otherwise.
 */
std::optional<nx::Url> NX_VMS_CLIENT_CORE_API getBookmarkSharingLink(
    const common::CameraBookmark& bookmark,
    SystemContext* context);

/**
 * Aggregate predicate intended for UI "shared bookmark" indication.
 * @param context Main system context is expected.
 * @return True when bookmark sharing feature is available, the bookmark itself is marked as
 *     shared, and the expiration did not occur by the moment of call. It's expected that a share
 *     link produced by getBookmarkSharingLink is followable if this predicate returns true.
 */
bool NX_VMS_CLIENT_CORE_API sharedBookmarkAccessible(const common::CameraBookmark& bookmark,
    SystemContext* context);

/**
 * @return Bookmark name derived from the object track, taxonomy-based.
 */
QString NX_VMS_CLIENT_CORE_API bookmarkNameFromObjectTrack(
    const nx::analytics::db::ObjectTrack& track,
    SystemContext* systemContext);

/**
 * @return Formatted bookmark description: object capture time, camera name and the given object
 *     attributes, one entry per line.
 */
QString NX_VMS_CLIENT_CORE_API bookmarkDescription(
    std::chrono::milliseconds startTime,
    const QnMediaResourcePtr& camera,
    const analytics::AttributeList& attributes);

/**
 * @return Camera bookmark structure initialized with data obtained from analytics object track,
 *     nullopt if invalid arguments were provided. Initialization of the creatorId and
 *     creationTimeStampMs fields is the responsibility of the caller.
 */
std::optional<common::CameraBookmark> NX_VMS_CLIENT_CORE_API bookmarkFromObjectTrack(
    const nx::analytics::db::ObjectTrack& track,
    const QnVirtualCameraResourcePtr& camera);

} // namespace bookmarks
} // namespace nx::vms::client::core
