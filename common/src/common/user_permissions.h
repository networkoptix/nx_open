#pragma once

#ifdef __cplusplus  // For safe iOS build
#include <utils/common/model_functions_fwd.h>
#endif

// TODO: #ynikitenkov Add serialization using metaobject
#ifndef QN_NO_NAMESPACES
namespace Qn
{
#endif

    /**
     * Flags describing the actions permitted for the user to do with the
     * selected resource. Calculated in runtime.
     */
    enum Permission
    {
        /* Generic permissions. */
        NoPermissions                   = 0x0000,   /**< No access */

        ReadPermission                  = 0x0001,   /**< Generic read access. Having this access right doesn't necessary mean that all information is readable. */
        WritePermission                 = 0x0002,   /**< Generic write access. Having this access right doesn't necessary mean that all information is writable. */
        SavePermission                  = 0x0004,   /**< Generic save access. Entity can be saved to the server. */
        RemovePermission                = 0x0008,   /**< Generic delete permission. */
        ReadWriteSavePermission = ReadPermission | WritePermission | SavePermission,
        WriteNamePermission             = 0x0010,   /**< Permission to edit resource's name. */

        /* Layout-specific permissions. */
        AddRemoveItemsPermission        = 0x0020,   /**< Permission to add or remove items from a layout. */
        EditLayoutSettingsPermission    = 0x0040,   /**< Permission to setup layout background or set locked flag. */
        FullLayoutPermissions = ReadWriteSavePermission | WriteNamePermission | RemovePermission | AddRemoveItemsPermission | EditLayoutSettingsPermission,

        /* User-specific permissions. */
        WritePasswordPermission         = 0x0200,   /**< Permission to edit associated password. */
        WriteAccessRightsPermission     = 0x0400,   /**< Permission to edit access rights. */
        CreateLayoutPermission          = 0x0800,   /**< Permission to create layouts for the user. */
        ReadEmailPermission = ReadPermission,
        WriteEmailPermission = WritePasswordPermission,

        /* Media-specific permissions. */
        ExportPermission                = 0x2000,   /**< Permission to export video parts. */

        /* Camera-specific permissions. */
        WritePtzPermission              = 0x1000,   /**< Permission to use camera's PTZ controls. */

        AllPermissions                  = 0xFFFFFFFF
    };

#ifdef __cplusplus
    Q_DECLARE_FLAGS(Permissions, Permission)
    Q_DECLARE_OPERATORS_FOR_FLAGS(Permissions)
#endif

    /**
     * Flags describing global user capabilities, independently of resources. Stored in the database.
     * QFlags uses int internally and don't have a constructor from quint64, so
     * if we place values bigger than uint max to the user permission, we should double-check all.
     */
    enum GlobalPermission
    {
        /* Generic permissions. */
        NoGlobalPermissions                     = 0x00000000,   /**< No access */

        GlobalEditProtectedUserPermission       = 0x00000001,   /**< Root, can edit admins. */
        GlobalProtectedPermission               = 0x00000002,   /**< Admin, can edit other non-admins. */
        GlobalEditLayoutsPermission             = 0x00000004,   /**< Can create and edit layouts. */
        GlobalEditUsersPermission               = 0x00000008,   /**< Can create and edit users. */
        GlobalEditServersPermissions            = 0x00000020,   /**< Can edit server settings. */
        GlobalViewLivePermission                = 0x00000080,   /**< Can view live stream of available cameras. */
        GlobalViewArchivePermission             = 0x00000100,   /**< Can view archives of available cameras. */
        GlobalExportPermission                  = 0x00000200,   /**< Can export archives of available cameras. */
        GlobalEditCamerasPermission             = 0x00000400,   /**< Can edit camera settings. */
        GlobalPtzControlPermission              = 0x00000800,   /**< Can change camera's PTZ state. */
        GlobalEditVideoWallPermission           = 0x00002000,   /**< Can create and edit videowalls */

        /* Deprecated permissions. */
        DeprecatedEditCamerasPermission         = 0x00000010,   /**< Can edit camera settings and change camera's PTZ state. */
        DeprecatedViewExportArchivePermission   = 0x00000040,   /**< Can view and export archives of available cameras. */

        /* Shortcuts. */
        GlobalLiveViewerPermissions             = GlobalViewLivePermission,
        GlobalViewerPermissions                 = GlobalLiveViewerPermissions       | GlobalViewArchivePermission | GlobalExportPermission,
        GlobalAdvancedViewerPermissions         = GlobalViewerPermissions           | GlobalEditCamerasPermission | GlobalPtzControlPermission,
        GlobalAdminPermissions                  = GlobalAdvancedViewerPermissions   | GlobalEditLayoutsPermission | GlobalEditUsersPermission |
                                                    GlobalProtectedPermission | GlobalEditServersPermissions | GlobalEditVideoWallPermission,
        GlobalOwnerPermissions                  = GlobalAdminPermissions            | GlobalEditProtectedUserPermission,
    };

#ifdef __cplusplus
    Q_DECLARE_FLAGS(GlobalPermissions, GlobalPermission)
    Q_DECLARE_OPERATORS_FOR_FLAGS(GlobalPermissions)
#endif


    /**
     * \param permissions               Permission flags containing some deprecated values.
     * \returns                         Permission flags with deprecated values replaced with new ones.
     */
    Qn::GlobalPermissions undeprecate(Qn::GlobalPermissions permissions);

    /* Some useful utility functions. */
    Qn::Permissions operator-(Qn::Permissions minuend, Qn::Permissions subrahend);
    Qn::Permissions operator-(Qn::Permissions minuend, Qn::Permission subrahend);
    Qn::Permissions operator-(Qn::Permission minuend, Qn::Permission subrahend);

#ifndef QN_NO_NAMESPACES
} // namespace Qn
#endif

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (Qn::Permission)(Qn::Permissions)(Qn::GlobalPermission)(Qn::GlobalPermissions),
    (lexical)
    )
