#ifndef QN_USER_PERMISSIONS_H
#define QN_USER_PERMISSIONS_H

#ifndef QN_NO_NAMESPACES
namespace Qn {
#endif

    /**
     * Flags describing the actions permitted for the user to do with the 
     * selected resource.
     */
    enum Permission {
        /* Generic permissions. */
        ReadPermission                          = 0x00010000,   /**< Generic read access. Having this access right doesn't necessary mean that all information is readable. */
        WritePermission                         = 0x00020000,   /**< Generic write access. Having this access right doesn't necessary mean that all information is writable. */ 
        SavePermission                          = 0x00040000,   /**< Generic save access. Entity can be saved to appserver. */
        RemovePermission                        = 0x00080000,   /**< Generic delete permission. */
        ReadWriteSavePermission                 = ReadPermission | WritePermission | SavePermission,
        WriteNamePermission                     = 0x01000000,   /**< Permission to edit resource's name. */

        /* Layout-specific permissions. */
        AddRemoveItemsPermission                = 0x00100000,   /**< Permission to add or remove items from a layout. */
        EditLayoutSettingsPermission            = 0x00200000,   /**< Permission to setup layout background or set locked flag. */
        FullLayoutPermissions                   = ReadWriteSavePermission | WriteNamePermission | RemovePermission | AddRemoveItemsPermission | EditLayoutSettingsPermission,
        
        /* User-specific permissions. */
        WritePasswordPermission                 = 0x02000000,   /**< Permission to edit associated password. */
        WriteAccessRightsPermission             = 0x04000000,   /**< Permission to edit access rights. */
        CreateLayoutPermission                  = 0x08000000,   /**< Permission to create layouts for the user. */
        ReadEmailPermission                     = ReadPermission,
        WriteEmailPermission                    = WritePasswordPermission,

        /* Media-specific permissions. */
        ExportPermission                        = 0x20000000,   /**< Permission to export video parts. */

        /* Camera-specific permissions. */
        WritePtzPermission                      = 0x10000000,   /**< Permission to use camera's PTZ controls. */

        /* Global permissions, applicable to current user only. */
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
        GlobalPanicPermission                   = 0x00001000,   /**< Can trigger panic recording. */
        GlobalEditVideoWallPermission           = 0x00002000,   /**< Can create and edit videowalls */
        
        /* Deprecated permissions. */
        DeprecatedEditCamerasPermission         = 0x00000010,   /**< Can edit camera settings and change camera's PTZ state. */
        DeprecatedViewExportArchivePermission   = 0x00000040,   /**< Can view and export archives of available cameras. */

        /* Shortcuts. */
        GlobalLiveViewerPermissions             = GlobalViewLivePermission,
        GlobalViewerPermissions                 = GlobalLiveViewerPermissions       | GlobalViewArchivePermission | GlobalExportPermission,
        GlobalAdvancedViewerPermissions         = GlobalViewerPermissions           | GlobalEditCamerasPermission | GlobalPtzControlPermission,
        GlobalAdminPermissions                  = GlobalAdvancedViewerPermissions   | GlobalEditLayoutsPermission | GlobalEditUsersPermission |
                                                    GlobalProtectedPermission | GlobalEditServersPermissions | GlobalPanicPermission | GlobalEditVideoWallPermission,
        GlobalOwnerPermissions                  = GlobalAdminPermissions            | GlobalEditProtectedUserPermission,
            
        AllPermissions                          = 0xFFFFFFFF
    };

#ifndef QN_NO_NAMESPACES
} // namespace Qn
#endif

#endif //QN_USER_PERMISSIONS_H
