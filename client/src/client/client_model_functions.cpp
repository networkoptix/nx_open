
#include <utils/common/model_functions.h>

#include <client/client_globals.h>
#include <client/client_meta_types.h>
#include <client/client_model_types.h>

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(Qn::ClientSkin,
    (Qn::LightSkin, "LightSkin")
    (Qn::DarkSkin, "DarkSkin")
);

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(Qn::BackgroundAnimationMode,
                                          (Qn::DefaultAnimation,    "DefaultAnimation")
                                          (Qn::RainbowAnimation,    "RainbowAnimation")
                                          (Qn::CustomAnimation,     "CustomAnimation")
                                          );

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(Qn::ImageBehaviour,
                                          (Qn::StretchImage,        "StretchImage")
                                          (Qn::CropImage,           "CropImage")
                                          (Qn::FitImage,            "FitImage")
                                          (Qn::TileImage,           "TileImage")
                                          );

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnWorkbenchState, (datastream), (currentLayoutIndex)(layoutUuids));
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnServerStorageKey, (datastream)(eq)(hash), (serverUuid)(storagePath));
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnLicenseWarningState, (datastream), (lastWarningTime));
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnClientBackground, (datastream), QnClientBackground_Fields);
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnPtzHotkey, (json), (id)(hotkey))

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(
    Qn::Permissions,
    (Qn::ReadPermission, "ReadPermission")
    (Qn::WritePermission, "WritePermission")
    (Qn::SavePermission, "SavePermission")
    (Qn::RemovePermission, "RemovePermission")
    (Qn::WriteNamePermission, "WriteNamePermission")
    (Qn::AddRemoveItemsPermission, "AddRemoveItemsPermission")
    (Qn::EditLayoutSettingsPermission, "EditLayoutSettingsPermission")
    (Qn::WritePasswordPermission, "WritePasswordPermission")
    (Qn::WriteAccessRightsPermission, "WriteAccessRightsPermission")
    (Qn::CreateLayoutPermission, "CreateLayoutPermission")
    (Qn::ExportPermission, "ExportPermission")
    (Qn::WritePtzPermission, "WritePtzPermission")
    (Qn::GlobalEditProtectedUserPermission, "GlobalEditProtectedUserPermission")
    (Qn::GlobalProtectedPermission, "GlobalProtectedPermission")
    (Qn::GlobalEditLayoutsPermission, "GlobalEditLayoutsPermission")
    (Qn::GlobalEditUsersPermission, "GlobalEditUsersPermission")
    (Qn::GlobalEditServersPermissions, "GlobalEditServersPermissions")
    (Qn::GlobalViewLivePermission, "GlobalViewLivePermission")
    (Qn::GlobalViewArchivePermission, "GlobalViewArchivePermission")
    (Qn::GlobalExportPermission, "GlobalExportPermission")
    (Qn::GlobalEditCamerasPermission, "GlobalEditCamerasPermission")
    (Qn::GlobalPtzControlPermission, "GlobalPtzControlPermission")
    (Qn::GlobalPanicPermission, "GlobalPanicPermission")
    (Qn::GlobalEditVideoWallPermission, "GlobalEditVideoWallPermission")
    (Qn::DeprecatedEditCamerasPermission, "DeprecatedEditCamerasPermission")
    (Qn::DeprecatedViewExportArchivePermission, "DeprecatedViewExportArchivePermission")
    );

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(
    Qn::Permission,
    (Qn::ReadPermission, "ReadPermission")
    (Qn::WritePermission, "WritePermission")
    (Qn::SavePermission, "SavePermission")
    (Qn::RemovePermission, "RemovePermission")
    (Qn::WriteNamePermission, "WriteNamePermission")
    (Qn::AddRemoveItemsPermission, "AddRemoveItemsPermission")
    (Qn::EditLayoutSettingsPermission, "EditLayoutSettingsPermission")
    (Qn::WritePasswordPermission, "WritePasswordPermission")
    (Qn::WriteAccessRightsPermission, "WriteAccessRightsPermission")
    (Qn::CreateLayoutPermission, "CreateLayoutPermission")
    (Qn::ExportPermission, "ExportPermission")
    (Qn::WritePtzPermission, "WritePtzPermission")
    (Qn::GlobalEditProtectedUserPermission, "GlobalEditProtectedUserPermission")
    (Qn::GlobalProtectedPermission, "GlobalProtectedPermission")
    (Qn::GlobalEditLayoutsPermission, "GlobalEditLayoutsPermission")
    (Qn::GlobalEditUsersPermission, "GlobalEditUsersPermission")
    (Qn::GlobalEditServersPermissions, "GlobalEditServersPermissions")
    (Qn::GlobalViewLivePermission, "GlobalViewLivePermission")
    (Qn::GlobalViewArchivePermission, "GlobalViewArchivePermission")
    (Qn::GlobalExportPermission, "GlobalExportPermission")
    (Qn::GlobalEditCamerasPermission, "GlobalEditCamerasPermission")
    (Qn::GlobalPtzControlPermission, "GlobalPtzControlPermission")
    (Qn::GlobalPanicPermission, "GlobalPanicPermission")
    (Qn::GlobalEditVideoWallPermission, "GlobalEditVideoWallPermission")
    (Qn::DeprecatedEditCamerasPermission, "DeprecatedEditCamerasPermission")
    (Qn::DeprecatedViewExportArchivePermission, "DeprecatedViewExportArchivePermission")
    );
