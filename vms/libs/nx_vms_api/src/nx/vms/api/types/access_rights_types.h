// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMetaType>

#include <nx/reflect/enum_instrument.h>
#include <nx/utils/serialization/flags.h>

namespace nx::vms::api {

/**%apidoc
 * Flags describing global user capabilities, independently of resources. Stored in the database.
 * QFlags uses int internally, so we are limited to 32 bits.
 */
enum class GlobalPermission
{

    /**%apidoc
     * Only live video access.
     * %caption NoGlobalPermissions
     */
    none = 0,

    /* Admin permissions. */

    /**%apidoc
     * Can edit other non-admins.
     * %caption GlobalAdminPermission
     */
     admin = 0x00000001,

    /* Manager permissions. */

    /**%apidoc
     * Can edit Device settings.
     * %caption GlobalEditCamerasPermission
     */
     editCameras = 0x00000002,

    /**%apidoc
     * Can control video-walls.
     * %caption GlobalControlVideoWallPermission
     */
     controlVideowall = 0x00000004,

    /**%apidoc
     * Can access Event Log and Audit Trail.
     * %caption GlobalViewLogsPermission
     */
     viewLogs = 0x00000010,

    /* Viewer permissions. */

    /**%apidoc
     * Can view archives of the available cameras.
     * %caption GlobalViewArchivePermission
     */
     viewArchive = 0x00000100,

    /**%apidoc
     * Can export archives of the available cameras.
     * %caption GlobalExportPermission
     */
     exportArchive = 0x00000200,

    /**%apidoc
     * Can view Bookmarks on the available cameras.
     * %caption GlobalViewBookmarksPermission
     */
     viewBookmarks = 0x00000400,

    /**%apidoc
     * Can modify Bookmarks on the available cameras.
     * %caption GlobalManageBookmarksPermission
     */
     manageBookmarks = 0x00000800,

    /* Input permissions. */

    /**%apidoc
     * Can change the camera PTZ state, use 2-way audio and I/O buttons.
     * %caption GlobalUserInputPermission
     */
     userInput = 0x00010000,

    /* Resources access permissions. */

    /**%apidoc
     * Has access to all media Resources (cameras and web pages).
     * %caption GlobalAccessAllMediaPermission
     */
     accessAllMedia = 0x01000000,

    /* Other permissions. */

    /**%apidoc
     * Just marks the new user as "custom".
     * %caption GlobalCustomUserPermission
     */
     customUser = 0x10000000,

    /**%apidoc[unused]
     * Owner permissions. This flag is for API handler registration only. Users in the DB have to
     * be checked by isOwner().
     */
    owner = 0x20000000,

    /**%apidoc[unused] */
    requireFreshSession = 0x40000000,

    /**%apidoc[unused] */
    adminWithFreshSession = admin | requireFreshSession,

    /**%apidoc[unused] */
    ownerWithFreshSession = owner | requireFreshSession,

    /* Combinations. */

    /**%apidoc[unused]
     * A Live Viewer has access to all Devices and global Layouts by default.
     */
    liveViewerPermissions = accessAllMedia,

    /**%apidoc[unused]
     * A Viewer can additionally view archive and Bookmarks, and export video.
     */
    viewerPermissions = liveViewerPermissions | viewArchive | exportArchive | viewBookmarks,

    /**%apidoc[unused]
     * An Advanced Viewer can manage Bookmarks and use various input methods.
     */
    advancedViewerPermissions = viewerPermissions | manageBookmarks | userInput | viewLogs,

    /**%apidoc[unused]
     * An Admin can do everything.
     */
    adminPermissions = admin | advancedViewerPermissions | controlVideowall | editCameras,

    /**%apidoc[unused]
     * Has access to all media. Can view archive and Bookmarks, change the camera PTZ state, use
     * 2-way audio and I/O buttons, and control video-walls.
     */
    videowallModePermissions = liveViewerPermissions | viewArchive | userInput
        | controlVideowall | viewBookmarks, /* PTZ here is intended - for SpaceX, see VMS-2208 */

    /**%apidoc[unused]
     * The actions in the ACS mode are limited.
     */
    acsModePermissions = viewerPermissions | userInput,
};

template<typename Visitor>
constexpr auto nxReflectVisitAllEnumItems(GlobalPermission*, Visitor&& visitor)
{
    using Item = nx::reflect::enumeration::Item<GlobalPermission>;
    return visitor(
        Item{GlobalPermission::none, "NoGlobalPermissions"},
        Item{GlobalPermission::admin, "GlobalAdminPermission"},
        Item{GlobalPermission::editCameras, "GlobalEditCamerasPermission"},
        Item{GlobalPermission::controlVideowall, "GlobalControlVideoWallPermission"},
        Item{GlobalPermission::viewLogs, "GlobalViewLogsPermission"},
        Item{GlobalPermission::viewArchive, "GlobalViewArchivePermission"},
        Item{GlobalPermission::exportArchive, "GlobalExportPermission"},
        Item{GlobalPermission::viewBookmarks, "GlobalViewBookmarksPermission"},
        Item{GlobalPermission::manageBookmarks, "GlobalManageBookmarksPermission"},
        Item{GlobalPermission::userInput, "GlobalUserInputPermission"},
        Item{GlobalPermission::accessAllMedia, "GlobalAccessAllMediaPermission"},
        Item{GlobalPermission::customUser, "GlobalCustomUserPermission"},
        Item{GlobalPermission::owner, "GlobalOwnerPermission"},
        Item{GlobalPermission::requireFreshSession, "GlobalRequireFreshSessionPermission"}
    );
}

Q_DECLARE_FLAGS(GlobalPermissions, GlobalPermission)
Q_DECLARE_OPERATORS_FOR_FLAGS(GlobalPermissions)

} // namespace nx::vms::api

Q_DECLARE_METATYPE(nx::vms::api::GlobalPermission)
Q_DECLARE_METATYPE(nx::vms::api::GlobalPermissions)
