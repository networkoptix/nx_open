#ifndef QN_CLIENT_GLOBALS_H
#define QN_CLIENT_GLOBALS_H

#include <QtCore/QMetaType>

namespace Qn {

    /** 
     * Type of a node in resource tree displayed to the user.
     */
    enum NodeType {
        RootNode,
        LocalNode,
        ServersNode,
        UsersNode,
        ResourceNode,   /**< Node that represents a resource. */
        ItemNode,       /**< Node that represents a layout item. */
        BastardNode,    /**< Node that contains hidden resources. */
        RecorderNode,   /**< Node that represents a recorder (VMAX, etc). */
        NodeTypeCount
    };


    /**
     * Role of an item on the scene. 
     * 
     * Note that at any time there may exist no more than one item for each role.
     * 
     * Also note that the order is important. Code in <tt>workbench.cpp</tt> relies on it.
     */
    enum ItemRole {
        ZoomedRole,         /**< Item is zoomed. */
        RaisedRole,         /**< Item is raised. */
        SingleSelectedRole, /**< Item is the only selected item on a workbench. */
        SingleRole,         /**< Item is the only item on a workbench. */
        ActiveRole,         /**< Item is active. */
        CentralRole,        /**< Item is 'central' --- zoomed, raised, single selected, or focused. */
        ItemRoleCount
    };
    

    /**
     * Item-specific flags. Are part of item's serializable state.
     */
    enum ItemFlag {
        Pinned = 0x1,                       /**< Item is pinned to the grid. Items are not pinned by default. */
        PendingGeometryAdjustment = 0x2     /**< Geometry adjustment is pending. 
                                             * Center of item's combined geometry defines desired position. 
                                             * If item's rect is invalid, but not empty (width or height are negative), then any position is OK. */
    };
    Q_DECLARE_FLAGS(ItemFlags, ItemFlag)
    Q_DECLARE_OPERATORS_FOR_FLAGS(ItemFlags)


    /**
     * Layer of a graphics item on the scene.
     * 
     * Workbench display presents convenience functions for moving items between layers
     * and guarantees that items from the layers with higher numbers are always
     * displayed on top of those from the layers with lower numbers.
     */
    enum ItemLayer {
        EMappingLayer,              /**< Layer for E-Mapping background. */
        BackLayer,                  /**< Back layer. */
        RaisedConeBgLayer,          /**< Layer for origin cone when item is not raised anymore. */
        PinnedLayer,                /**< Layer for pinned items. */
        RaisedConeLayer,            /**< Layer for origin cone for raised items. */
        PinnedRaisedLayer,          /**< Layer for pinned items that are raised. */
        UnpinnedLayer,              /**< Layer for unpinned items. */
        UnpinnedRaisedLayer,        /**< Layer for unpinned items that are raised. */
        ZoomedLayer,                /**< Layer for zoomed items. */
        FrontLayer,                 /**< Topmost layer for items. Items that are being dragged, resized or manipulated in any other way are to be placed here. */
        EffectsLayer,               /**< Layer for top-level effects. */
        UiLayer,                    /**< Layer for ui elements, i.e. navigation bar, resource tree, etc... */
        MessageBoxLayer,            /**< Layer for graphics text messages. */
        LayerCount
    };


    /**
     * Flags describing how viewport margins affect viewport geometry.
     */
    enum MarginFlag {
        /** Viewport margins affect how viewport size is bounded. */
        MarginsAffectSize = 0x1,        

        /** Viewport margins affect how viewport position is bounded. */
        MarginsAffectPosition = 0x2
    };
    Q_DECLARE_FLAGS(MarginFlags, MarginFlag)
    Q_DECLARE_OPERATORS_FOR_FLAGS(MarginFlags)


    /**
     * Flags describing the differences between instances of the same resource
     * on the client and on the enterprise controller.
     */
    enum ResourceSavingFlag {
        /** Resource is local and has never been saved to EC. */
        ResourceIsLocal = 0x1,

        /** Resource is currently being saved to EC. */
        ResourceIsBeingSaved = 0x2,

        /** Unsaved changes are present in the resource. */
        ResourceIsChanged = 0x4
    };
    Q_DECLARE_FLAGS(ResourceSavingFlags, ResourceSavingFlag)
    Q_DECLARE_OPERATORS_FOR_FLAGS(ResourceSavingFlags)


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
        FullLayoutPermissions                   = ReadWriteSavePermission | WriteNamePermission | Qn::RemovePermission | AddRemoveItemsPermission | EditLayoutSettingsPermission,
        
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
        
        /* Deprecated permissions. */
        DeprecatedEditCamerasPermission         = 0x00000010,   /**< Can edit camera settings and change camera's PTZ state. */
        DeprecatedViewExportArchivePermission   = 0x00000040,   /**< Can view and export archives of available cameras. */

        /* Shortcuts. */
        GlobalLiveViewerPermissions             = GlobalViewLivePermission,
        GlobalViewerPermissions                 = GlobalLiveViewerPermissions       | GlobalViewArchivePermission | GlobalExportPermission,
        GlobalAdvancedViewerPermissions         = GlobalViewerPermissions           | GlobalEditCamerasPermission | GlobalPtzControlPermission,
        GlobalAdminPermissions                  = GlobalAdvancedViewerPermissions   | GlobalEditLayoutsPermission | GlobalEditUsersPermission | GlobalProtectedPermission | GlobalEditServersPermissions | GlobalPanicPermission,
        GlobalOwnerPermissions                  = GlobalAdminPermissions            | GlobalEditProtectedUserPermission,
            
        AllPermissions                          = 0xFFFFFFFF
    };
    Q_DECLARE_FLAGS(Permissions, Permission)
    Q_DECLARE_OPERATORS_FOR_FLAGS(Permissions)


    /**
     * \param permissions               Permission flags containing some deprecated values.
     * \returns                         Permission flags with deprecated values replaced with new ones.
     */
    inline Qn::Permissions undeprecate(Qn::Permissions permissions) {
        Qn::Permissions result = permissions;

        if(result & Qn::DeprecatedEditCamerasPermission) {
            result &= ~Qn::DeprecatedEditCamerasPermission;
            result |= Qn::GlobalEditCamerasPermission | Qn::GlobalPtzControlPermission;
        }

        if(result & Qn::DeprecatedViewExportArchivePermission) {
            result &= ~Qn::DeprecatedViewExportArchivePermission;
            result |= Qn::GlobalViewArchivePermission | Qn::GlobalExportPermission;
        }

        if(result & Qn::GlobalProtectedPermission)
            result |= Qn::GlobalPanicPermission;

        return result;
    }


    /**
     * Time display mode. 
     */
    enum TimeMode {
        ServerTimeMode, 
        ClientTimeMode  
    };

    /**
     * Columns in the resource tree model.
     */
    enum ResourceTreeColumn {
        NameColumn,
        CheckColumn,
        ColumnCount
    };

    /**
     * Overlay for resource widgets.
     */
    enum ResourceStatusOverlay {
        EmptyOverlay,
        PausedOverlay,
        LoadingOverlay,
        NoDataOverlay,
        UnauthorizedOverlay,
        OfflineOverlay,
        AnalogWithoutLicenseOverlay,
        ServerOfflineOverlay,

        OverlayCount
    };

    /**
     * Result of a frame rendering operation. 
     * 
     * Note that the order is important here --- higher values are prioritized
     * when calculating cumulative status of several rendering operations.
     */
    enum RenderStatus {
        NothingRendered,    /**< No frames to render, so nothing was rendered. */
        CannotRender,       /**< Something went wrong. */
        OldFrameRendered,   /**< No new frames available, old frame was rendered. */
        NewFrameRendered    /**< New frame was rendered. */
    };


    /**
     * Video resolution adjustment mode for RADASS.
     */
    enum ResolutionMode {
        AutoResolution,
        HighResolution,
        LowResolution,
        ResolutionModeCount
    };

    /**
     * Importance level of a notification. 
     */
    enum NotificationLevel {
        NoNotification,
        OtherNotification,
        CommonNotification,
        ImportantNotification,
        CriticalNotification,
        SystemNotification,
        SoundNotification,

        LevelCount
    };

} // namespace Qn

Q_DECLARE_TYPEINFO(Qn::ItemRole, Q_PRIMITIVE_TYPE);
Q_DECLARE_METATYPE(Qn::ItemRole)
Q_DECLARE_METATYPE(Qn::TimeMode)

#endif // QN_CLIENT_GLOBALS_H
