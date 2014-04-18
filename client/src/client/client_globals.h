#ifndef QN_CLIENT_GLOBALS_H
#define QN_CLIENT_GLOBALS_H

#include <QtCore/QMetaType>

namespace Qn {

    /** 
     * Type of a node in resource tree displayed to the user.
     */
    enum NodeType {
        RootNode,               /**< Root node for the tree. */
        LocalNode,              /**< Root node for local resources. */
        ServersNode,            /**< Root node for remote resources. */
        UsersNode,              /**< Root node for user resources. */

        BastardNode,            /**< Root node for hidden resources. */

        ResourceNode,           /**< Node that represents a resource. Has only resource. */
        ItemNode,               /**< Node that represents a layout item. Has both guid and resource. */
        RecorderNode,           /**< Node that represents a recorder (VMAX, etc). Has both guid and resource (parent server). */
        EdgeNode,               /**< Node that represents an EDGE server with a camera. Has only resource - server's only camera. */

        VideoWallItemNode,      /**< Node that represents a video wall item. Has a guid and can have resource. */
        VideoWallHistoryNode,   /**< Node that represents a video wall history item. Has both guid and resource. */

        UserVideoWallNode,      /**< Node that represents a video wall having items that user is allowed to edit. Has both guid and resource. */
        UserVideoWallItemNode,  /**< Node that represents a video wall item that user is allowed to edit. Has a guid. */

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


    #include "user_permissions.h"
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
        LevelCount
    };

    /**
     * Modes of layout export.
     */
    enum LayoutExportMode {
        LayoutLocalSave,
        LayoutLocalSaveAs,
        LayoutExport
    };

    /**
     * Flags describing the client light mode.
     */
    enum LightModeFlag {
        LightModeNoAnimation        = 0x0001,           /**< Disable all client animations. */
        LightModeSmallWindow        = 0x0002,           /**< Decrease minimum window size. */
        LightModeNoSceneBackground  = 0x0004,           /**< Disable gradient scene background. */
        LightModeNoOpacity          = 0x0008,           /**< Disable opacity in ui widgets. */
        LightModeNoNotifications    = 0x0010,           /**< Disable notifications panel. */
        LightModeSingleItem         = 0x0020,           /**< Limit number of simultaneous items on the scene to 1. */
        LightModeNoShadows          = 0x0040,           /**< Disable shadows on ui elements. */
        LightModeNoMultisampling    = 0x0080,           /**< Disable OpenGL multisampling. */
        LightModeNoNewWindow        = 0x0100,           /**< Disable opening of new windows. */
        LightModeNoLayoutBackground = 0x0200,           /**< Disable layout background. */
        LightModeNoZoomWindows      = 0x0400,           /**< Disable zoom windows. */

        LightModeVideoWall          = LightModeNoSceneBackground | LightModeNoNotifications | LightModeNoShadows /*| LightModeNoAnimation*/,
        LightModeFull               = 0xFFFFFFFF

    };
    Q_DECLARE_FLAGS(LightModeFlags, LightModeFlag)
    Q_DECLARE_OPERATORS_FOR_FLAGS(LightModeFlags)


    enum ClientSkin {
        DarkSkin,
        LightSkin
    };


} // namespace Qn

Q_DECLARE_TYPEINFO(Qn::ItemRole, Q_PRIMITIVE_TYPE);
Q_DECLARE_METATYPE(Qn::ItemRole)
Q_DECLARE_METATYPE(Qn::TimeMode)
Q_DECLARE_METATYPE(Qn::ClientSkin)
Q_DECLARE_TYPEINFO(Qn::NodeType, Q_PRIMITIVE_TYPE);
Q_DECLARE_METATYPE(Qn::NodeType)

#endif // QN_CLIENT_GLOBALS_H
