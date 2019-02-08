#include "user_permissions.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace common {
namespace compatibility {
namespace user_permissions {

GlobalPermissions migrateFromV26(GlobalPermissionsV26 oldPermissions)
{
    // Admin permission should be enough for everything, it is automatically expanded on check.
    if (oldPermissions.testFlag(V26OwnerPermission) || oldPermissions.testFlag(V26AdminPermission))
        return GlobalPermission::admin;

    // First step: check 2.0 permissions.
    if (oldPermissions.testFlag(V20EditCamerasPermission))
        oldPermissions |= V26EditCamerasPermission | V26PtzControlPermission;

    if (oldPermissions.testFlag(V20ViewExportArchivePermission))
        oldPermissions |= V26ViewArchivePermission | V26ExportPermission;

    // Second step: process v2.5 permissions.
    GlobalPermissions result;
    if (oldPermissions.testFlag(V26EditCamerasPermission))
    {
        result |= GlobalPermission::editCameras;
        // Advanced viewers will be able to edit bookmarks.
        if (oldPermissions.testFlag(V26ViewArchivePermission))
            result |= GlobalPermission::manageBookmarks;
    }

    if (oldPermissions.testFlag(V26PtzControlPermission))
        result |= GlobalPermission::userInput;

    if (oldPermissions.testFlag(V26ViewArchivePermission))
        result |= GlobalPermission::viewArchive | GlobalPermission::viewBookmarks;

    if (oldPermissions.testFlag(V26ExportPermission))
    {
        result |= GlobalPermission::viewArchive
            | GlobalPermission::viewBookmarks
            | GlobalPermission::exportArchive;
    }

    if (oldPermissions.testFlag(V26EditVideoWallPermission))
        result |= GlobalPermission::controlVideowall;

    return result;
}

} // namespace user_permissions
} // namespace compatibility
} // namespace common
} // namespace nx

using namespace nx::common::compatibility::user_permissions;

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(
    nx::common::compatibility::user_permissions, GlobalPermissionV26,
    (GlobalPermissionV26::V26OwnerPermission, "v26_owner")
    (GlobalPermissionV26::V26AdminPermission, "v26_admin")
    (GlobalPermissionV26::V26EditLayoutsPermission, "v26_editLayouts")
    (GlobalPermissionV26::V26EditServersPermissions, "v26_editServers")
    (GlobalPermissionV26::V26ViewLivePermission, "v26_viewLive")
    (GlobalPermissionV26::V26ViewArchivePermission, "v26_viewArchive")
    (GlobalPermissionV26::V26ExportPermission, "v26_export")
    (GlobalPermissionV26::V26EditCamerasPermission, "v26_editCameras")
    (GlobalPermissionV26::V26PtzControlPermission, "v26_ptzControl")
    (GlobalPermissionV26::V26EditVideoWallPermission, "v26_editVideowall")
    (GlobalPermissionV26::V20EditUsersPermission, "v20_editUsers")
    (GlobalPermissionV26::V20EditCamerasPermission, "v20_editCameras")
    (GlobalPermissionV26::V20ViewExportArchivePermission, "v20_viewArchive")
    (GlobalPermissionV26::V20PanicPermission, "v20_panic")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(
    nx::common::compatibility::user_permissions, GlobalPermissionsV26,
    (GlobalPermissionV26::V26OwnerPermission, "v26_owner")
    (GlobalPermissionV26::V26AdminPermission, "v26_admin")
    (GlobalPermissionV26::V26EditLayoutsPermission, "v26_editLayouts")
    (GlobalPermissionV26::V26EditServersPermissions, "v26_editServers")
    (GlobalPermissionV26::V26ViewLivePermission, "v26_viewLive")
    (GlobalPermissionV26::V26ViewArchivePermission, "v26_viewArchive")
    (GlobalPermissionV26::V26ExportPermission, "v26_export")
    (GlobalPermissionV26::V26EditCamerasPermission, "v26_editCameras")
    (GlobalPermissionV26::V26PtzControlPermission, "v26_ptzControl")
    (GlobalPermissionV26::V26EditVideoWallPermission, "v26_editVideowall")
    (GlobalPermissionV26::V20EditUsersPermission, "v20_editUsers")
    (GlobalPermissionV26::V20EditCamerasPermission, "v20_editCameras")
    (GlobalPermissionV26::V20ViewExportArchivePermission, "v20_viewArchive")
    (GlobalPermissionV26::V20PanicPermission, "v20_panic")
)
