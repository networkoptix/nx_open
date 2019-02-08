#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <common/common_globals.h>

namespace nx {
namespace common {
namespace compatibility {
namespace user_permissions {

/** Permissions from v2.5 */
enum GlobalPermissionV26
{
    V26OwnerPermission          = 0x0001,   /**< Root, can edit admins. */
    V26AdminPermission          = 0x0002,   /**< Admin, can edit other non-admins. */
    V26EditLayoutsPermission    = 0x0004,   /**< Can create and edit layouts. */
    V26EditServersPermissions   = 0x0020,   /**< Can edit server settings. */
    V26ViewLivePermission       = 0x0080,   /**< Can view live stream of available cameras. */
    V26ViewArchivePermission    = 0x0100,   /**< Can view archives of available cameras. */
    V26ExportPermission         = 0x0200,   /**< Can export archives of available cameras. */
    V26EditCamerasPermission    = 0x0400,   /**< Can edit camera settings. */
    V26PtzControlPermission     = 0x0800,   /**< Can change camera's PTZ state. */
    V26EditVideoWallPermission  = 0x2000,   /**< Can create and edit videowalls */

    /* These permissions were deprecated in 2.5 but kept for compatibility reasons. */
    V20EditUsersPermission      = 0x0008,
    V20EditCamerasPermission    = 0x0010,
    V20ViewExportArchivePermission = 0x0040,
    V20PanicPermission          = 0x1000,
};
Q_DECLARE_FLAGS(GlobalPermissionsV26, GlobalPermissionV26)
Q_DECLARE_OPERATORS_FOR_FLAGS(GlobalPermissionsV26)

GlobalPermissions migrateFromV26(GlobalPermissionsV26 oldPermissions);

} // namespace user_permissions
} // namespace compatibility
} // namespace common
} // namespace nx

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (nx::common::compatibility::user_permissions::GlobalPermissionV26)
    (nx::common::compatibility::user_permissions::GlobalPermissionsV26),
    (lexical))
