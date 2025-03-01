// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <iostream>

#include <nx/fusion/fusion/fusion_fwd.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/utils/auth/global_permission.h>
#include <nx/utils/json/flags.h>
#include <nx/utils/uuid.h>

namespace nx::vms::api {

using GlobalPermission = nx::utils::auth::GlobalPermission;
using GlobalPermissions = nx::utils::auth::GlobalPermissions;

constexpr GlobalPermissions kAssignableGlobalPermissions =
    GlobalPermission::viewLogs
    | GlobalPermission::generateEvents
    | GlobalPermission::viewMetrics
    | GlobalPermission::viewUnredactedVideo;

// -------------------------------------------------------------------------------------------------

/**%apidoc
 * Flags describing User access rights towards particular Resources.
 * %// Stored in the database. QFlags uses int internally, so we are limited to 32 bits.
 */
NX_REFLECTION_ENUM_CLASS(AccessRight,

    /**%apidoc Can see a resource. Can access live footage from a media resource. */
    view = 1 << 0,

    /**%apidoc Can access an audio stream from a Device. */
    audio = 1 << 1,

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
    | AccessRight::audio
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
    std::vector<nx::Uuid> groupIds;

    /**%apidoc[opt] All inherited permissions. */
    GlobalPermissions permissions = GlobalPermission::none;

    /**%apidoc[opt]
     * All inherited access rights per Resource (can be obtained from `/rest/v{3-}/devices`,
     * `/rest/v{3-}/servers`, etc.) or Resource Group (can be obtained from
     * `/rest/v{3-}/resourceGroups`).
     */
    std::map<nx::Uuid, AccessRights> resourceAccessRights;
};
#define PermissionsModel_Fields (groupIds)(permissions)(resourceAccessRights)

QN_FUSION_DECLARE_FUNCTIONS(PermissionsModel, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(PermissionsModel, PermissionsModel_Fields)

// Special resource group ids.
NX_VMS_API extern const nx::Uuid kAllDevicesGroupId;
NX_VMS_API extern const nx::Uuid kAllWebPagesGroupId;
NX_VMS_API extern const nx::Uuid kAllServersGroupId;
NX_VMS_API extern const nx::Uuid kAllVideoWallsGroupId;

NX_VMS_API std::optional<SpecialResourceGroup> specialResourceGroup(const nx::Uuid& id);
NX_VMS_API nx::Uuid specialResourceGroupId(SpecialResourceGroup group);

// GoogleTest printers.
NX_VMS_API void PrintTo(GlobalPermission value, std::ostream* os);
NX_VMS_API void PrintTo(GlobalPermissions value, std::ostream* os);
NX_VMS_API void PrintTo(AccessRight value, std::ostream* os);
NX_VMS_API void PrintTo(AccessRights value, std::ostream* os);
NX_VMS_API void PrintTo(SpecialResourceGroup value, std::ostream* os);

} // namespace nx::vms::api
