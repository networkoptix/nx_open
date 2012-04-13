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


} // namespace Qn

Q_DECLARE_TYPEINFO(Qn::ItemRole, Q_PRIMITIVE_TYPE);
Q_DECLARE_OPERATORS_FOR_FLAGS(Qn::ItemFlags);
Q_DECLARE_OPERATORS_FOR_FLAGS(Qn::Borders);
Q_DECLARE_OPERATORS_FOR_FLAGS(Qn::LayoutFlags);


#endif // QN_WORKBENCH_GLOBALS_H
