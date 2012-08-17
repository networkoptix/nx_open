#ifndef QN_WORKBENCH_GLOBALS_H
#define QN_WORKBENCH_GLOBALS_H

#include <QtCore/QtGlobal>

namespace Qn {
    /**
     * Role of an item on the scene. 
     * 
     * Note that at any time there may exist no more than a single item for each role.
     */
    enum ItemRole {
        SingleSelectedRole, /**< Item is the only selected item on a workbench. */
        RaisedRole,         /**< Item is raised. */
        ZoomedRole,         /**< Item is zoomed. */
        SingleRole,         /**< Item is the only item on a workbench. */
        CentralRole,        /**< Item is 'central' --- zoomed, raised, single selected, or single. */
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
    Q_DECLARE_FLAGS(ItemFlags, ItemFlag);

    /**
     * Layout-specific flags.
     */
    //enum LayoutFlags {
    //    LayoutIsFile = 0x1,                 /**< Layout was opened from a MultiStream file. */
    //    LayoutIsQuickSearch = 0x2,          /**< Layout is a QuickSearch view of some resource. */
    //};


    /**
     * Layer of a graphics item on the scene.
     * 
     * Workbench display presents convenience functions for moving items between layers
     * and guarantees that items from the layers with higher numbers are always
     * displayed on top of those from the layers with lower numbers.
     */
    enum ItemLayer {
        BackLayer,                  /**< Back layer. */
        PinnedLayer,                /**< Layer for pinned items. */
        PinnedRaisedLayer,          /**< Layer for pinned items that are raised. */
        UnpinnedLayer,              /**< Layer for unpinned items. */
        UnpinnedRaisedLayer,        /**< Layer for unpinned items that are raised. */
        CurtainLayer,               /**< Layer for curtain that blacks out the background when an item is zoomed. */
        ZoomedLayer,                /**< Layer for zoomed items. */
        FrontLayer,                 /**< Topmost layer for items. Items that are being dragged, resized or manipulated in any other way are to be placed here. */
        EffectsLayer,               /**< Layer for top-level effects. */
        UiLayer,                    /**< Layer for ui elements, i.e. navigation bar, resource tree, etc... */
    };


    /**
     * Generic enumeration describing borders of a rectangle.
     */
    enum Border {
        NoBorders = 0,
        LeftBorder = 0x1,
        RightBorder = 0x2,
        TopBorder = 0x4,
        BottomBorder = 0x8,
        AllBorders = LeftBorder | RightBorder | TopBorder | BottomBorder
    };
    Q_DECLARE_FLAGS(Borders, Border);


    /**
     * Flags describing how viewport margins affect viewport geometry.
     */
    enum MarginFlag {
        /** Viewport margins affect how viewport size is bounded. */
        MarginsAffectSize = 0x1,        

        /** Viewport margins affect how viewport position is bounded. */
        MarginsAffectPosition = 0x2,
    };
    Q_DECLARE_FLAGS(MarginFlags, MarginFlag);


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
        ResourceIsChanged = 0x4,
    };
    Q_DECLARE_FLAGS(ResourceSavingFlags, ResourceSavingFlag);


    /**
     * Flags describing the actions permitted for the user. 
     */
    enum Permission {
        /* Generic permissions. */

        /** Generic read access. Having this access right doesn't necessary mean that all information is readable. */
        ReadPermission              = 0x00000001,   

        /** Generic write access. Having this access right doesn't necessary mean that all information is writable. */ 
        WritePermission             = 0x00000002,   

        /** Generic save access. Entity can be saved to appserver. */
        SavePermission              = 0x00000004,   

        /** Generic delete permission. */
        RemovePermission            = 0x00000008,

        /** Generic read-write-save permission. */
        ReadWriteSavePermission     = ReadPermission | WritePermission | SavePermission,


        /* Layout-specific permissions. */

        /** Permission to add or remove items from a layout. */
        AddRemoveItemsPermission    = 0x00000010,


        /* User-specific permissions. */

        /** Permission to edit login. */
        WriteLoginPermission        = 0x00000100, // TODO: replace with generic WriteNamePermission

        /** Permission to edit associated password. */
        WritePasswordPermission     = 0x00000200,

        /** Permission to edit access rights. */
        WriteAccessRightsPermission = 0x00000400,

        /** Permission to create layouts for the user. */
        CreateLayoutPermission      = 0x00000800,


        /* Current user-specific permissions. Are meaningful for a resource representing current user only. */

        /** Permission to create users. */
        CreateUserPermission        = 0x10000000,


        AllPermissions              = 0xFFFFFFFF
    };
    Q_DECLARE_FLAGS(Permissions, Permission)

    enum UserRight{
        EditLayoutRight             = 0x00000001,
        ProtectedRight              = 0x00000002,
        EditProtectedUserRight      = 0x00000004,
        EditUserRight               = 0x00000008,
        EditCameraRight             = 0x00000010,
        EditServerRight             = 0x00000020,

        /* Combined rights */
        AdminRight                  = EditLayoutRight | EditUserRight | ProtectedRight | EditCameraRight | EditServerRight,
        SuperUserRight              = AdminRight | EditProtectedUserRight

    };
    Q_DECLARE_FLAGS(UserRights, UserRight)


} // namespace Qn

Q_DECLARE_TYPEINFO(Qn::ItemRole, Q_PRIMITIVE_TYPE);
Q_DECLARE_OPERATORS_FOR_FLAGS(Qn::ItemFlags);
Q_DECLARE_OPERATORS_FOR_FLAGS(Qn::Borders);
Q_DECLARE_OPERATORS_FOR_FLAGS(Qn::MarginFlags);
Q_DECLARE_OPERATORS_FOR_FLAGS(Qn::ResourceSavingFlags);
Q_DECLARE_OPERATORS_FOR_FLAGS(Qn::Permissions);


#endif // QN_WORKBENCH_GLOBALS_H
