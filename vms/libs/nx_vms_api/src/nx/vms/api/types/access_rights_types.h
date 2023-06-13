// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <iostream>

#include <nx/reflect/enum_instrument.h>
#include <nx/utils/serialization/flags.h>
#include <nx/utils/uuid.h>

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
     * %// Will be superseded by AccessRight::view. The description should be changed to "No global
     *     permissions" then.
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

    /**%apidoc
     * Can edit Device settings of available devices.
     * %caption GlobalEditCamerasPermission
     * %// Will be superseded by AccessRight::edit.
     */
    editCameras = 0x00000002,

    /**%apidoc
     * Can control all video-walls in the system.
     * %caption GlobalControlVideoWallPermission
     * %// Will be superseded by AccessRight::view for particular videowall resources.
     */
    controlVideowall = 0x00000004,

    /**%apidoc
     * Can access Event Log and Audit Trail.
     * %caption GlobalViewLogsPermission
     */
    viewLogs = 0x00000010,

    /**%apidoc
     * Can access System Health Monitoring information.
     * %caption GlobalSystemHealthPermission
     * %// This is a new flag, it shouldn't be copied to DeprecatedGlobalPermission.
     */
     systemHealth = 0x00000020,

    /* Viewer permissions. */

    /**%apidoc
     * Can view archives of the available cameras.
     * %caption GlobalViewArchivePermission
     * %// Will be superseded by AccessRight::viewArchive.
     */
    viewArchive = 0x00000100,

    /**%apidoc
     * Can export archives of the available cameras.
     * %caption GlobalExportPermission
     * %// Will be superseded by AccessRight::exportArchive.
     */
    exportArchive = 0x00000200,

    /**%apidoc
     * Can view Bookmarks on the available cameras.
     * %caption GlobalViewBookmarksPermission
     * %// Will be superseded by AccessRight::viewBookmarks.
     */
    viewBookmarks = 0x00000400,

    /**%apidoc
     * Can modify Bookmarks on the available cameras.
     * %caption GlobalManageBookmarksPermission
     * %// Will be superseded by AccessRight::manageBookmarks.
     */
    manageBookmarks = 0x00000800,

    /* Input permissions. */

    /**%apidoc
     * Can change the camera PTZ state, use 2-way audio and I/O buttons.
     * %caption GlobalUserInputPermission
     * %// Will be superseded by AccessRight::userInput.
     */
    userInput = 0x00010000,

    /**%apidoc
     * Can generate VMS Events.
     * %caption GlobalGenerateEventsPermission
     * %// This is a new flag, it shouldn't be copied to DeprecatedGlobalPermission.
     */
     generateEvents = 0x00020000,

    /* Resources access permissions. */

    /**%apidoc
     * Has access to all media Resources (cameras and web pages).
     * %caption GlobalAccessAllMediaPermission
     * %// Will be superseded by AccessRight::view for kAllDevicesGroupId, kAllWebPagesGroupId and
     *     kAllServersGroupId.
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

    /**%apidoc[unused]
     * A Live Viewer has access to all Devices and global Layouts by default.
     * %// Will be deprecated with the new User Groups.
     */
    liveViewerPermissions = accessAllMedia,

    /**%apidoc[unused]
     * A Viewer can additionally view archive and Bookmarks, and export video.
     * %// Will be deprecated with the new User Groups.
     */
    viewerPermissions = liveViewerPermissions | viewArchive | exportArchive | viewBookmarks,

    /**%apidoc[unused]
     * An Advanced Viewer can manage Bookmarks and use various input methods.
     * %// Will be deprecated with the new User Groups.
     */
    advancedViewerPermissions = viewerPermissions | manageBookmarks | userInput | viewLogs,

    /**%apidoc[unused]
     * An Admin can do everything.
     * %// Will be deprecated with the new User Groups.
     */
    powerUserPermissions = powerUser | advancedViewerPermissions | controlVideowall | editCameras,

    /**%apidoc[unused]
     * Has access to all media. Can view archive and Bookmarks, change the camera PTZ
     * state, use 2-way audio and I/O buttons, and control video-walls.
     * %// Will be deprecated with the new User Groups.
     */
    videowallModePermissions = liveViewerPermissions | viewArchive | userInput
        | controlVideowall | viewBookmarks, /* PTZ here is intended - for SpaceX, see VMS-2208 */

    /**%apidoc[unused]
     * The actions in the ACS mode are limited.
     * %// Will be deprecated with the new User Groups.
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
        Item{GlobalPermission::systemHealth, "GlobalSystemHealthPermission"},
        Item{GlobalPermission::viewArchive, "GlobalViewArchivePermission"},
        Item{GlobalPermission::exportArchive, "GlobalExportPermission"},
        Item{GlobalPermission::viewBookmarks, "GlobalViewBookmarksPermission"},
        Item{GlobalPermission::manageBookmarks, "GlobalManageBookmarksPermission"},
        Item{GlobalPermission::userInput, "GlobalUserInputPermission"},
        Item{GlobalPermission::generateEvents, "GlobalGenerateEventsPermission"},
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
    | GlobalPermission::generateEvents
    | GlobalPermission::systemHealth
    | GlobalPermission::customUser
    | GlobalPermission::requireFreshSession;

constexpr GlobalPermissions kAssignableGlobalPermissions =
    GlobalPermission::viewLogs
    | GlobalPermission::generateEvents
    | GlobalPermission::customUser;

constexpr GlobalPermissions kDeprecatedGlobalPermissions{~kNonDeprecatedGlobalPermissions};

/**%apidoc
 * Flags describing User access rights towards particular Resources.
 * %// Stored in the database. QFlags uses int internally, so we are limited to 32 bits.
 */
NX_REFLECTION_ENUM_CLASS(AccessRight,

    /**%apidoc Can see a resource. Can access live footage from a media resource. */
    view = 0x0001,

    /**%apidoc Can view archive. */
    viewArchive = 0x0004,

    /**%apidoc Can export archive. */
    exportArchive = 0x0008,

    /**%apidoc Can view Bookmarks. */
    viewBookmarks = 0x0010,

    /**%apidoc Can modify Bookmarks. */
    manageBookmarks = 0x0020,

    /**%apidoc Can change the camera PTZ state, use 2-way audio and I/O buttons. */
    userInput = 0x0080,

    /**%apidoc Can edit Device settings. */
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

// Special resource group ids.
NX_VMS_API extern const QnUuid kAllDevicesGroupId;
NX_VMS_API extern const QnUuid kAllWebPagesGroupId;
NX_VMS_API extern const QnUuid kAllServersGroupId;
NX_VMS_API extern const QnUuid kAllVideoWallsGroupId;

NX_VMS_API std::optional<SpecialResourceGroup> specialResourceGroup(const QnUuid& id);
NX_VMS_API QnUuid specialResourceGroupId(SpecialResourceGroup group);

// GoogleTest printers.
NX_VMS_API void PrintTo(GlobalPermission value, std::ostream* os);
NX_VMS_API void PrintTo(GlobalPermissions value, std::ostream* os);
NX_VMS_API void PrintTo(AccessRight value, std::ostream* os);
NX_VMS_API void PrintTo(AccessRights value, std::ostream* os);
NX_VMS_API void PrintTo(SpecialResourceGroup value, std::ostream* os);

} // namespace nx::vms::api
