#pragma once

#include <QtCore/QMetaType>

#include <nx/fusion/model_functions_fwd.h>

namespace Qn
{
    /**
     * Type of a node in resource tree displayed to the user. Ordered as it should be displayed on the screen.
     */
    enum NodeType
    {
        // Single-occurrence nodes
        RootNode,               /**< Root node for the tree (current system node). */
        CurrentSystemNode,      /**< Root node, displaying current system name. */
        SeparatorNode,          /**< Root node for spacing between header and main part of the tree. */
        ServersNode,            /**< Root node for servers for admin user. */
        UserDevicesNode,        /**< Root node for cameras and i/o modules for non-admin user. */
        LayoutsNode,            /**< Root node for current user's layouts and shared layouts. */
        WebPagesNode,           /**< Root node for web pages. */
        UsersNode,              /**< Root node for user resources. */
        LocalResourcesNode,     /**< Root node for local resources. */
        LocalSeparatorNode,     /**< Root node for spacing between local resources header and resources. */
        OtherSystemsNode,       /**< Root node for remote resources which are incompatible with current system and cannot be used. */

        BastardNode,            /**< Root node for hidden resources. */

        // Per-user placeholder nodes
        AllCamerasAccessNode,   /**< Node that represents 'All Cameras' placeholder, displayed under users and roles with full access. */
        AllLayoutsAccessNode,   /**< Node that represents 'All Shared Layouts' placeholder, displayed under admins. */
        AccessibleResourcesNode,/**< Node that represents 'Cameras & Resources' node, displayed under users and roles with custom access. */
        AccessibleLayoutsNode,  /**< Node that represents 'Layouts' node, displayed under users and roles with custom access. */
        RoleUsersNode,          /**< Node that represents 'Users' node, displayed under roles. */

        // Repeating nodes
        RoleNode,               /**< Node that represents custom role. */
        SharedLayoutNode,       /**< Node that represents shared layout link, displayed under user. Has only resource - shared layout. */
        AccessibleResourceNode, /**< Node that represents accessible resource link, displayed under user. Has only resource - camera or web page. */
        RecorderNode,           /**< Node that represents a recorder (VMAX, etc). Has both guid and resource (parent server). */
        ResourceNode,           /**< Node that represents a resource. Has only resource. */
        LayoutItemNode,         /**< Node that represents a layout item. Has both guid and resource. */
        EdgeNode,               /**< Node that represents an EDGE server with a camera. Has only resource - server's only camera. */

        VideoWallItemNode,      /**< Node that represents a videowall item. Has a guid and can have resource. */
        VideoWallMatrixNode,    /**< Node that represents a videowall saved matrix. Has a guid. */

        SystemNode,             /**< Node that represents systems but the current. */

        NodeTypeCount
    };

    inline bool isSeparatorNode(NodeType t) { return t == SeparatorNode || t == LocalSeparatorNode;  }

    /**
     * Role of an item on the scene.
     *
     * Note that at any time there may exist no more than one item for each role.
     *
     * Also note that the order is important. Code in <tt>workbench.cpp</tt> relies on it.
     */
    enum ItemRole
    {
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
    enum ItemFlag
    {
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
    enum ItemLayer
    {
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
    enum MarginFlag
    {
        /** Viewport margins affect how viewport size is bounded. */
        MarginsAffectSize = 0x1,

        /** Viewport margins affect how viewport position is bounded. */
        MarginsAffectPosition = 0x2
    };
    Q_DECLARE_FLAGS(MarginFlags, MarginFlag)
    Q_DECLARE_OPERATORS_FOR_FLAGS(MarginFlags)

    /**
     * Flags describing the differences between instances of the same resource
     * on the client and on the Server.
     */
    enum ResourceSavingFlag
    {
        /** Resource is currently being saved to Server. */
        ResourceIsBeingSaved = 0x1,

        /** Unsaved changes are present in the resource. */
        ResourceIsChanged = 0x2
    };
    Q_DECLARE_FLAGS(ResourceSavingFlags, ResourceSavingFlag)
    Q_DECLARE_OPERATORS_FOR_FLAGS(ResourceSavingFlags)

    /**
     * Time display mode.
     */
    enum TimeMode
    {
        ServerTimeMode,
        ClientTimeMode
    };

    /**
     * Columns in the resource tree model.
     */
    enum ResourceTreeColumn
    {
        NameColumn,     /**< Resource icon and name. */
        CustomColumn,   /**< Additional column for custom information. */
        CheckColumn,    /**< Checkbox for resource selection. */
        ColumnCount
    };

    /**
     * Overlay for resource widgets.
     */
    enum ResourceStatusOverlay
    {
        EmptyOverlay,
        PausedOverlay,
        LoadingOverlay,
        NoDataOverlay,
        NoVideoDataOverlay,
        UnauthorizedOverlay,
        OfflineOverlay,
        AnalogWithoutLicenseOverlay,
        VideowallWithoutLicenseOverlay,
        ServerOfflineOverlay,
        ServerUnauthorizedOverlay,
        IoModuleDisabledOverlay,

        OverlayCount
    };

    /**
     * Result of a frame rendering operation.
     *
     * Note that the order is important here --- higher values are prioritized
     * when calculating cumulative status of several rendering operations.
     */
    enum RenderStatus
    {
        NothingRendered,    /**< No frames to render, so nothing was rendered. */
        CannotRender,       /**< Something went wrong. */
        OldFrameRendered,   /**< No new frames available, old frame was rendered. */
        NewFrameRendered    /**< New frame was rendered. */
    };

    /**
     * Video resolution adjustment mode for RADASS.
     */
    enum ResolutionMode
    {
        AutoResolution,
        HighResolution,
        LowResolution,
        ResolutionModeCount
    };

    /**
     * Modes of layout export.
     */
    enum LayoutExportMode
    {
        LayoutLocalSave,
        LayoutLocalSaveAs,
        LayoutExport
    };

    /**
     * Flags describing the client light mode.
     */
    enum LightModeFlag
    {
        LightModeNoAnimation        = 0x0001,           /**< Disable all client animations. */
        LightModeSmallWindow        = 0x0002,           /**< Decrease minimum window size. */
        LightModeNoSceneBackground  = 0x0004,           /**< Disable gradient scene background. */

        LightModeNoNotifications    = 0x0010,           /**< Disable notifications panel. */
        LightModeSingleItem         = 0x0020,           /**< Limit number of simultaneous items on the scene to 1. */
        LightModeNoShadows          = 0x0040,           /**< Disable shadows on ui elements. */
        LightModeNoMultisampling    = 0x0080,           /**< Disable OpenGL multisampling. */
        LightModeNoNewWindow        = 0x0100,           /**< Disable opening of new windows. */
        LightModeNoLayoutBackground = 0x0200,           /**< Disable layout background. */
        LightModeNoZoomWindows      = 0x0400,           /**< Disable zoom windows. */

        LightModeActiveX            = LightModeSmallWindow | LightModeNoSceneBackground
                                    | LightModeNoNotifications | LightModeNoShadows
                                    | LightModeNoNewWindow | LightModeNoLayoutBackground
                                    | LightModeNoZoomWindows,
        LightModeVideoWall          = LightModeNoSceneBackground | LightModeNoNotifications | LightModeNoShadows,
        LightModeFull               = 0x7FFFFFFF

    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(LightModeFlag)
    Q_DECLARE_FLAGS(LightModeFlags, LightModeFlag)
    Q_DECLARE_OPERATORS_FOR_FLAGS(LightModeFlags)

    enum ClientSkin
    {
        DarkSkin,
        LightSkin
    };

    enum BackgroundAnimationMode
    {
        DefaultAnimation,
        RainbowAnimation,
        CustomAnimation
    };

    enum ImageBehaviour
    {
        StretchImage,
        CropImage,
        FitImage,
        TileImage
    };

    /**
    Workbench pane state
    */
    enum class PaneState
    {
        Unpinned,       /**< pane is unpinned and closed. Timeline is unpinned and opened. */
        Opened,         /**< pane is pinned and opened. */
        Closed          /**< pane is pinned and closed. Timeline is unpinned and closed.  */
    };

    /**
    Workbench pane id enumeration
    */
    enum class WorkbenchPane
    {
        Title,          /**< title pane         */
        Tree,           /**< resource tree pane */
        Notifications,  /**< notifications pane */
        Navigation,     /**< navigation pane    */
        Calendar,       /**< calendar pane      */
        Thumbnails      /**< thumbnails pane    */
    };

    /**
     * Flags for messages that should be displayed to user only once
     * (usually with 'Do not show anymore' checkbox).
     */
    enum class ShowOnceMessage
    {
        PtzPresetInUse              = 0x001,    /**< Delete ptz preset which is used in the tour. */
        SharedLayoutEdit            = 0x002,    /**< Edit shared layout. */
        ChangeUserLocalLayout       = 0x004,    /**< Items are removed from user's layout, but access still persist. */
        AddToRoleLocalLayout        = 0x008,    /**< Items are added to roled user's layout. */
        RemoveFromRoleLocalLayout   = 0x010,    /**< Items are removed from roled user's layout, but access still persist. */
        DeleteResources             = 0x020,    /**< Batch delete resources (but layouts). */
        DeleteLocalLayouts          = 0x040     /**< Batch delete user's or group's local layouts. */
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(ShowOnceMessage)
    Q_DECLARE_FLAGS(ShowOnceMessages, ShowOnceMessage)
    Q_DECLARE_OPERATORS_FOR_FLAGS(ShowOnceMessages)

} // namespace Qn

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (Qn::ItemRole)(Qn::TimeMode)(Qn::NodeType),
    (metatype)
    )

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (Qn::ClientSkin)(Qn::BackgroundAnimationMode)(Qn::ImageBehaviour)(Qn::PaneState)(Qn::WorkbenchPane),
    (metatype)(lexical)(datastream)
    )

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (Qn::LightModeFlags)(Qn::ShowOnceMessages),
    (metatype)(numeric)
    )


//TODO: #GDM #2.4 move back to client_model_functions.cpp. See commit a0edb9aa5abede1b5c9ab1c9ce52cc912286d090.
//Looks like problem is in gcc-4.8.1

inline QDataStream &operator<<(QDataStream &stream, const Qn::BackgroundAnimationMode &value)
{
    return stream << static_cast<int>(value);
}

inline QDataStream &operator>>(QDataStream &stream, Qn::BackgroundAnimationMode &value)
{
    int tmp;
    stream >> tmp;
    value = static_cast<Qn::BackgroundAnimationMode>(tmp);
    return stream;
}

inline QDataStream &operator<<(QDataStream &stream, const Qn::ImageBehaviour &value)
{
    return stream << static_cast<int>(value);
}

inline QDataStream &operator>>(QDataStream &stream, Qn::ImageBehaviour &value)
{
    int tmp;
    stream >> tmp;
    value = static_cast<Qn::ImageBehaviour>(tmp);
    return stream;
}

