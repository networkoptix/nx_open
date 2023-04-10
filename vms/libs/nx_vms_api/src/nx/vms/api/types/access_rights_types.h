// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once


#include <nx/reflect/enum_instrument.h>
#include <nx/utils/serialization/flags.h>

namespace nx::vms::api {

/**%apidoc
 * Flags describing global user capabilities, independently of resources. Stored in the database.
 * QFlags uses int internally, so we are limited to 32 bits.
 */
enum class GlobalPermission
{
    // Will be superseded by AccessRight::view.
    // Description should be changed to "No global permissions" then.
    /**%apidoc
     * Only live video access.
     * %caption NoGlobalPermissions
     */
    none = 0,

    /* Administrative permissions. */

    /**%apidoc[unused]
     * Administrator permissions: full control of the VMS system.
     * Not directly assignable, only inherited from the Administrators predefined group.
     */
    administrator = 0x20000000, //< Former `owner`.

    /**%apidoc[unused]
     * Management of devices and non-power users.
     * Not directly assignable, only inherited from the Power Users predefined group.
     */
    powerUser = 0x00000001, //< Former `admin`.

    /* Manager permissions. */

    // Will be superseded by AccessRight::edit.
    /**%apidoc
     * Can edit Device settings of available devices.
     * %caption GlobalEditCamerasPermission
     */
    editCameras = 0x00000002,

    // Will be superseded by AccessRight::view for particular videowall resources.
    /**%apidoc
     * Can control all video-walls in the system.
     * %caption GlobalControlVideoWallPermission
     */
    controlVideowall = 0x00000004,

    /**%apidoc
     * Can access Event Log and Audit Trail.
     * %caption GlobalViewLogsPermission
     */
    viewLogs = 0x00000010,

    /* Viewer permissions. */

    // Will be superseded by AccessRight::viewArchive.
    /**%apidoc
     * Can view archives of the available cameras.
     * %caption GlobalViewArchivePermission
     */
    viewArchive = 0x00000100,

    // Will be superseded by AccessRight::exportArchive.
    /**%apidoc
     * Can export archives of the available cameras.
     * %caption GlobalExportPermission
     */
    exportArchive = 0x00000200,

    // Will be superseded by AccessRight::viewBookmarks.
    /**%apidoc
     * Can view Bookmarks on the available cameras.
     * %caption GlobalViewBookmarksPermission
     */
    viewBookmarks = 0x00000400,

    // Will be superseded by AccessRight::manageBookmarks.
    /**%apidoc
     * Can modify Bookmarks on the available cameras.
     * %caption GlobalManageBookmarksPermission
     */
    manageBookmarks = 0x00000800,

    /* Input permissions. */

    // Will be superseded by AccessRight::userInput.
    /**%apidoc
     * Can change the camera PTZ state, use 2-way audio and I/O buttons.
     * %caption GlobalUserInputPermission
     */
    userInput = 0x00010000,

    /* Resources access permissions. */

    // Will be superseded by AccessRight::view for kAllDevicesGroupId, kAllWebPagesGroupId and kAllServersGroupId.
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

    /**%apidoc[unused] */
    requireFreshSession = 0x40000000,

    /**%apidoc[unused] */
    powerUserWithFreshSession = powerUser | requireFreshSession,

    /**%apidoc[unused] */
    administratorWithFreshSession = administrator | requireFreshSession,

    /* Combinations. */

    // Will be deprecated with new user groups.
    /**%apidoc[unused]
     * A Live Viewer has access to all Devices and global Layouts by default.
     */
    liveViewerPermissions = accessAllMedia,

    // Will be deprecated with new user groups.
    /**%apidoc[unused]
     * A Viewer can additionally view archive and Bookmarks, and export video.
     */
    viewerPermissions = liveViewerPermissions | viewArchive | exportArchive | viewBookmarks,

    // Will be deprecated with new user groups.
    /**%apidoc[unused]
     * An Advanced Viewer can manage Bookmarks and use various input methods.
     */
    advancedViewerPermissions = viewerPermissions | manageBookmarks | userInput | viewLogs,

    // Will be deprecated with new user groups.
    /**%apidoc[unused]
     * An Admin can do everything.
     */
    powerUserPermissions = powerUser | advancedViewerPermissions | controlVideowall | editCameras,

    // Will be deprecated with new user groups.
    /**%apidoc[unused]
     * Has access to all media. Can view archive and Bookmarks, change the camera PTZ
     * state, use 2-way audio and I/O buttons, and control video-walls.
     */
    videowallModePermissions = liveViewerPermissions | viewArchive | userInput
        | controlVideowall | viewBookmarks, /* PTZ here is intended - for SpaceX, see VMS-2208 */

    // Will be deprecated with new user groups.
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
        // For compatibility with existing APIs we should serialize this value to
        // "GlobalAdminPermission" by default.
        Item{GlobalPermission::powerUser, "GlobalAdminPermission"},
        Item{GlobalPermission::powerUser, "GlobalPowerUserPermission"},
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
        // For compatibility with existing APIs we should serialize this value to
        // "GlobalOwnerPermission" by default.
        Item{GlobalPermission::administrator, "GlobalOwnerPermission"},
        Item{GlobalPermission::administrator, "GlobalAdministratorPermission"},
        Item{GlobalPermission::requireFreshSession, "GlobalRequireFreshSessionPermission"});
}

Q_DECLARE_FLAGS(GlobalPermissions, GlobalPermission)
Q_DECLARE_OPERATORS_FOR_FLAGS(GlobalPermissions)

constexpr GlobalPermissions kNonDeprecatedGlobalPermissions =
    GlobalPermission::powerUser
    | GlobalPermission::administrator
    | GlobalPermission::viewLogs
    | GlobalPermission::customUser
    | GlobalPermission::requireFreshSession;

constexpr GlobalPermissions kAssignableGlobalPermissions =
    GlobalPermission::viewLogs
    | GlobalPermission::customUser;

constexpr GlobalPermissions kDeprecatedGlobalPermissions{~kNonDeprecatedGlobalPermissions};

// Stored in the database. QFlags uses int internally, so we are limited to 32 bits.
/**%apidoc
 * Flags describing User access rights towards particular Resources.
 */
NX_REFLECTION_ENUM_CLASS(AccessRight,

    /**%apidoc
     * Can see a resource. Can access live footage from a media resource.
     */
    view = 0x0001,

    /**%apidoc
     * Can view archive.
     */
    viewArchive = 0x0004,

    /**%apidoc
     * Can export archive.
     */
    exportArchive = 0x0008,

    /**%apidoc
     * Can view Bookmarks.
     */
    viewBookmarks = 0x0010,

    /**%apidoc
     * Can modify Bookmarks.
     */
    manageBookmarks = 0x0020,

    /**%apidoc
     * Can change the camera PTZ state, use 2-way audio and I/O buttons.
     */
    userInput = 0x0080,

    /**%apidoc
     * Can edit Device settings.
     */
    edit = 0x0200
)

Q_DECLARE_FLAGS(AccessRights, AccessRight)
Q_DECLARE_OPERATORS_FOR_FLAGS(AccessRights)

static constexpr AccessRights kNoAccessRights{};
static constexpr AccessRights kViewAccessRights{AccessRight::view};

static constexpr AccessRights kFullAccessRights{
    AccessRight::view
    | AccessRight::viewArchive
    | AccessRight::exportArchive
    | AccessRight::viewBookmarks
    | AccessRight::manageBookmarks
    | AccessRight::userInput
    | AccessRight::edit};

NX_REFLECTION_ENUM_CLASS(SpecialResourceGroup,
    allDevices,
    allWebPages,
    allServers,
    allVideowalls
)

} // namespace nx::vms::api
