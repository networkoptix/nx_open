// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <iostream>

#include <nx/fusion/fusion/fusion_fwd.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/utils/serialization/flags.h>
#include <nx/utils/uuid.h>

namespace nx::vms::api {

/**%apidoc
 * Flags describing global User capabilities, independently of Resources.
 * %// Stored in the database. QFlags uses int internally, so we are limited to 32 bits.
 */
enum class GlobalPermission
{
    /**%apidoc
     * No global permissions.
     */
    none = 0,

    /* Administrative permissions. */

    /**%apidoc[unused]
     * Administrator permissions: full control of the VMS system.
     * Not directly assignable, only inherited from the Administrators predefined group.
     */
    administrator = 0x20000000,

    /**%apidoc[unused]
     * Management of devices and non-power users.
     * Not directly assignable, only inherited from the Power Users predefined group.
     */
    powerUser = 0x00000001,

    /**%apidoc
     * Can access Event Log and Audit Trail.
     */
    viewLogs = 0x00000010,

    /**%apidoc
     * Can access System Health Monitoring information.
     */
    viewMetrics = 0x00000020,

    /**%apidoc
     * Can generate VMS Events.
     */
    generateEvents = 0x00020000,

    /**%apidoc[unused] */
    requireFreshSession = 0x40000000,

    /**%apidoc[unused] */
    powerUserWithFreshSession = powerUser | requireFreshSession,

    /**%apidoc[unused] */
    administratorWithFreshSession = administrator | requireFreshSession,
};
NX_REFLECTION_INSTRUMENT_ENUM(GlobalPermission,
    none,
    administrator,
    powerUser,
    viewLogs,
    viewMetrics,
    generateEvents)

Q_DECLARE_FLAGS(GlobalPermissions, GlobalPermission)
Q_DECLARE_OPERATORS_FOR_FLAGS(GlobalPermissions)

constexpr GlobalPermissions kAssignableGlobalPermissions =
    GlobalPermission::viewLogs | GlobalPermission::generateEvents | GlobalPermission::viewMetrics;

// -------------------------------------------------------------------------------------------------

/**%apidoc
 * Flags describing User access rights towards particular Resources.
 * %// Stored in the database. QFlags uses int internally, so we are limited to 32 bits.
 */
NX_REFLECTION_ENUM_CLASS(AccessRight,

    /**%apidoc Can see a resource. Can access live footage from a media resource. */
    view = 1 << 0,

    /**%apidoc Can view archive. */
    viewArchive = 1 << 2,

    /**%apidoc Can export archive. */
    exportArchive = 1 << 3,

    /**%apidoc Can view Bookmarks. */
    viewBookmarks = 1 << 4,

    /**%apidoc Can modify Bookmarks. */
    manageBookmarks = 1 << 5,

    /**%apidoc Can change the camera PTZ state, use 2-way audio and I/O buttons. */
    userInput = 1 << 7,

    /**%apidoc Can edit Device settings. */
    edit = 1 << 9
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

// -------------------------------------------------------------------------------------------------

struct NX_VMS_API PermissionsModel
{
    /**%apidoc All inherited group ids, can be obtained from `GET /rest/v{3-}/userGroups` */
    std::vector<QnUuid> groupIds;

    /**%apidoc[opt] All inherited permissions. */
    GlobalPermissions permissions = GlobalPermission::none;

    /**%apidoc[opt]
     * All inherited access rights per Resource (can be obtained from `/rest/v{3-}/devices`,
     * `/rest/v{3-}/servers`, etc.) or Resource Group (can be obtained from
     * `/rest/v{3-}/resourceGroups`).
     */
    std::map<QnUuid, AccessRights> resourceAccessRights;
};
#define PermissionsModel_Fields (groupIds)(permissions)(resourceAccessRights)

QN_FUSION_DECLARE_FUNCTIONS(PermissionsModel, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(PermissionsModel, PermissionsModel_Fields)

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
