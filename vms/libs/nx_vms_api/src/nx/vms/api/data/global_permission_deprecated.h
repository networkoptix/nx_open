// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <optional>
#include <vector>

#include <nx/utils/uuid.h>

#include "../types/access_rights_types.h"

namespace nx::vms::api {

/**%apidoc
 * Flags describing global user capabilities, independently of resources. Stored in the database.
 * QFlags uses int internally, so we are limited to 32 bits.
 */
enum class GlobalPermissionDeprecated
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

    /* Combinations matched to predefined groups. */

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
};

template<typename Visitor>
constexpr auto nxReflectVisitAllEnumItems(GlobalPermissionDeprecated*, Visitor&& visitor)
{
    using Item = nx::reflect::enumeration::Item<GlobalPermissionDeprecated>;
    return visitor(Item{GlobalPermissionDeprecated::none, "NoGlobalPermissions"},
        Item{GlobalPermissionDeprecated::admin, "GlobalAdminPermission"},
        Item{GlobalPermissionDeprecated::editCameras, "GlobalEditCamerasPermission"},
        Item{GlobalPermissionDeprecated::controlVideowall, "GlobalControlVideoWallPermission"},
        Item{GlobalPermissionDeprecated::viewLogs, "GlobalViewLogsPermission"},
        Item{GlobalPermissionDeprecated::viewArchive, "GlobalViewArchivePermission"},
        Item{GlobalPermissionDeprecated::exportArchive, "GlobalExportPermission"},
        Item{GlobalPermissionDeprecated::viewBookmarks, "GlobalViewBookmarksPermission"},
        Item{GlobalPermissionDeprecated::manageBookmarks, "GlobalManageBookmarksPermission"},
        Item{GlobalPermissionDeprecated::userInput, "GlobalUserInputPermission"},
        Item{GlobalPermissionDeprecated::accessAllMedia, "GlobalAccessAllMediaPermission"},
        Item{GlobalPermissionDeprecated::customUser, "GlobalCustomUserPermission"});
}

Q_DECLARE_FLAGS(GlobalPermissionsDeprecated, GlobalPermissionDeprecated)
Q_DECLARE_OPERATORS_FOR_FLAGS(GlobalPermissionsDeprecated)

/** Returns modernPermissions, groupIds, resourceAccessRights. */
NX_VMS_API std::tuple<GlobalPermissions, std::vector<QnUuid>, std::map<QnUuid, AccessRights>>
    migrateAccessRights(
        GlobalPermissionsDeprecated permissions,
        const std::vector<QnUuid>& accessibleResources,
        bool isOwner = false);

/** Returns deprecatedPermissions, groupId, isOwner. */
NX_VMS_API std::tuple<GlobalPermissionsDeprecated, std::optional<std::vector<QnUuid>>, bool>
    extractFromResourceAccessRights(
        GlobalPermissions permissions,
        std::vector<QnUuid>* groups,
        const std::map<QnUuid, AccessRights>& resourceAccessRights);

} // namespace nx::vms::api
