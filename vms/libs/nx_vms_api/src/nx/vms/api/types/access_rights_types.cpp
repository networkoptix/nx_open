#include "access_rights_types.h"

#include <nx/fusion/model_functions.h>

QN_FUSION_DEFINE_FUNCTIONS(nx::vms::api::GlobalPermission, (numeric)(debug))
QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api, GlobalPermission,
    (nx::vms::api::GlobalPermission::none, "NoGlobalPermissions")
    (nx::vms::api::GlobalPermission::admin, "GlobalAdminPermission")
    (nx::vms::api::GlobalPermission::editCameras, "GlobalEditCamerasPermission")
    (nx::vms::api::GlobalPermission::controlVideowall, "GlobalControlVideoWallPermission")
    (nx::vms::api::GlobalPermission::viewLogs, "GlobalViewLogsPermission")
    (nx::vms::api::GlobalPermission::viewArchive, "GlobalViewArchivePermission")
    (nx::vms::api::GlobalPermission::exportArchive, "GlobalExportPermission")
    (nx::vms::api::GlobalPermission::viewBookmarks, "GlobalViewBookmarksPermission")
    (nx::vms::api::GlobalPermission::manageBookmarks, "GlobalManageBookmarksPermission")
    (nx::vms::api::GlobalPermission::userInput, "GlobalUserInputPermission")
    (nx::vms::api::GlobalPermission::accessAllMedia, "GlobalAccessAllMediaPermission")
    (nx::vms::api::GlobalPermission::customUser, "GlobalCustomUserPermission"))

QN_FUSION_DEFINE_FUNCTIONS(nx::vms::api::GlobalPermissions, (numeric)(debug))
QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api, GlobalPermissions,
    (nx::vms::api::GlobalPermission::none, "NoGlobalPermissions")
    (nx::vms::api::GlobalPermission::admin, "GlobalAdminPermission")
    (nx::vms::api::GlobalPermission::editCameras, "GlobalEditCamerasPermission")
    (nx::vms::api::GlobalPermission::controlVideowall, "GlobalControlVideoWallPermission")
    (nx::vms::api::GlobalPermission::viewLogs, "GlobalViewLogsPermission")
    (nx::vms::api::GlobalPermission::viewArchive, "GlobalViewArchivePermission")
    (nx::vms::api::GlobalPermission::exportArchive, "GlobalExportPermission")
    (nx::vms::api::GlobalPermission::viewBookmarks, "GlobalViewBookmarksPermission")
    (nx::vms::api::GlobalPermission::manageBookmarks, "GlobalManageBookmarksPermission")
    (nx::vms::api::GlobalPermission::userInput, "GlobalUserInputPermission")
    (nx::vms::api::GlobalPermission::accessAllMedia, "GlobalAccessAllMediaPermission")
    (nx::vms::api::GlobalPermission::customUser, "GlobalCustomUserPermission"))
