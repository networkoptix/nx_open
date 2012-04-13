#ifndef QN_WORKBENCH_GLOBALS_H
#define QN_WORKBENCH_GLOBALS_H

#include <QtCore/QtGlobal>

namespace Qn {
    enum ItemRole {
        RaisedRole,  /**< The item is raised. */
        ZoomedRole,  /**< The item is zoomed. */
        ItemRoleCount
    };
    

    enum ItemFlag {
        Pinned = 0x1,                       /**< Item is pinned to the grid. Items are not pinned by default. */
        PendingGeometryAdjustment = 0x2     /**< Geometry adjustment is pending. 
                                             * Center of item's combined geometry defines desired position. 
                                             * If item's rect is invalid, but not empty (width or height are negative), then any position is OK. */
    };
    Q_DECLARE_FLAGS(ItemFlags, ItemFlag);


    /**
     * Layer of an item.
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
        UiLayer,                    /**< Layer for ui elements, i.e. close button, navigation bar, etc... */
    };


    enum Border {
        NoBorders = 0,
        LeftBorder = 0x1,
        RightBorder = 0x2,
        TopBorder = 0x4,
        BottomBorder = 0x8,
        AllBorders = LeftBorder | RightBorder | TopBorder | BottomBorder
    };
    Q_DECLARE_FLAGS(Borders, Border);


    enum LayoutFlag {
        /** Layout is local and was never saved to appserver. */
        LayoutIsLocal = 0x1,

        /** Layout is currently being saved to appserver. */
        LayoutIsBeingSaved = 0x2,

        /** LayoutIsLocal unsaved changes are present in the layout. */
        LayoutIsChanged = 0x4,
    };
    Q_DECLARE_FLAGS(LayoutFlags, LayoutFlag);


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



        /* User-specific permissions. */

        /** Permission to edit login. */
        WriteLoginPermission        = 0x00000100, // TODO: replace with generic WriteNamePermission

        /** Permission to edit associated password. */
        WritePasswordPermission     = 0x00000200,

        /** Permission to edit access rights. */
        WriteAccessRightsPermission = 0x00000400,

        /** Permission to create layouts for the user. */
        CreateLayoutPermission      = 0x00002000,



        /* Global permissions. */

        /** Permission to create users. */
        CreateUserPermission        = 0x00001000,

        AllPermissions              = 0xFFFFFFFF
    };
    Q_DECLARE_FLAGS(Permissions, Permission);


} // namespace Qn

Q_DECLARE_TYPEINFO(Qn::ItemRole, Q_PRIMITIVE_TYPE);
Q_DECLARE_OPERATORS_FOR_FLAGS(Qn::ItemFlags);
Q_DECLARE_OPERATORS_FOR_FLAGS(Qn::Borders);
Q_DECLARE_OPERATORS_FOR_FLAGS(Qn::LayoutFlags);
Q_DECLARE_OPERATORS_FOR_FLAGS(Qn::Permissions);


#endif // QN_WORKBENCH_GLOBALS_H
