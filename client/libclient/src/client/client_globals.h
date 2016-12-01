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
        CurrentUserNode,        /**< Root node, displaying current user. */
        SeparatorNode,          /**< Root node for spacing between header and main part of the tree. */
        MyCloudNode,            /**< Root node for available cloud systems. */
        ServersNode,            /**< Root node for servers for admin user. */
        UserResourcesNode,      /**< Root node for cameras, i/o modules and statistics for non-admin user. */
        LayoutsNode,            /**< Root node for current user's layouts and shared layouts. */
        WebPagesNode,           /**< Root node for web pages. */
        UsersNode,              /**< Root node for user resources. */
        LocalResourcesNode,     /**< Root node for local resources. */
        LocalSeparatorNode,     /**< Root node for spacing between local resources header and resources. */
        OtherSystemsNode,       /**< Root node for remote resources which are incompatible with current system and cannot be used. */

        BastardNode,            /**< Root node for hidden resources. */

        // Per-user placeholder nodes
        RoleUsersNode,          /**< Node that represents 'Users' node, displayed under roles. */
        AllCamerasAccessNode,   /**< Node that represents 'All Cameras' placeholder, displayed under users and roles with full access. */
        AllLayoutsAccessNode,   /**< Node that represents 'All Shared Layouts' placeholder, displayed under admins. */
        SharedResourcesNode,    /**< Node that represents 'Cameras & Resources' node, displayed under users and roles with custom access. */
        SharedLayoutsNode,      /**< Node that represents 'Layouts' node, displayed under users and roles with custom access. */

        // Repeating nodes
        RoleNode,               /**< Node that represents custom role. */
        SharedLayoutNode,       /**< Node that represents shared layout link, displayed under user. Has only resource - shared layout. */
        SharedResourceNode,     /**< Node that represents accessible resource link, displayed under user. Has only resource - camera or web page. */
        RecorderNode,           /**< Node that represents a recorder (VMAX, etc). Has both guid and resource (parent server). */
        ResourceNode,           /**< Node that represents a resource. Has only resource. */
        LayoutItemNode,         /**< Node that represents a layout item. Has both guid and resource. */
        EdgeNode,               /**< Node that represents an EDGE server with a camera. Has only resource - server's only camera. */

        VideoWallItemNode,      /**< Node that represents a videowall item. Has a guid and can have resource. */
        VideoWallMatrixNode,    /**< Node that represents a videowall saved matrix. Has a guid. */

        SystemNode,             /**< Node that represents systems but the current. */
        CloudSystemNode,        /**< Node that represents available cloud system. */

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
    * Generic enumeration holding different data roles used in Qn classes.
    */
    enum ItemDataRole
    {
        FirstItemDataRole = Qt::UserRole,

        /* Tree-based. */
        NodeTypeRole,                               /**< Role for node type, see <tt>Qn::NodeType</tt>. */

        /* Resource-based. */
        ResourceRole,                               /**< Role for QnResourcePtr. */
        UserResourceRole,                           /**< Role for QnUserResourcePtr. */
        LayoutResourceRole,                         /**< Role for QnLayoutResourcePtr. */
        MediaServerResourceRole,                    /**< Role for QnMediaServerResourcePtr. */
        VideoWallResourceRole,                      /**< Role for QnVideoWallResourcePtr */

        ResourceNameRole,                           /**< Role for resource name. Value of type QString. */
        ResourceFlagsRole,                          /**< Role for resource flags. Value of type int (Qn::ResourceFlags). */
        ResourceSearchStringRole,                   /**< Role for resource search string. Value of type QString. */
        ResourceStatusRole,                         /**< Role for resource status. Value of type int (Qn::ResourceStatus). */
        ResourceUidRole,                            /**< Role for resource unique id. Value of type QString. */

        VideoWallGuidRole,                          /**< Role for videowall resource unique id. Value of type QnUuid. */
        VideoWallItemGuidRole,                      /**< Role for videowall item unique id. Value of type QnUuid. */
        VideoWallItemIndicesRole,                   /**< Role for videowall item indices list. Value of type QnVideoWallItemIndexList. */

        /* Layout-based. */
        LayoutCellSpacingRole,                      /**< Role for layout's cell spacing. Value of type qreal. */
        LayoutCellAspectRatioRole,                  /**< Role for layout's cell aspect ratio. Value of type qreal. */
        LayoutBoundingRectRole,                     /**< Role for layout's bounding rect. Value of type QRect. */
        LayoutSyncStateRole,                        /**< Role for layout's stream synchronization state. Value of type QnStreamSynchronizationState. */
        LayoutSearchStateRole,                      /**< Role for 'Preview Search' layout parameters. */
        LayoutTimeLabelsRole,                       /**< Role for layout's time label display. Value of type bool. */
        LayoutPermissionsRole,                      /**< Role for overriding layout's permissions. Value of type int (Qn::Permissions). */
        LayoutSelectionRole,                        /**< Role for layout's selected items. Value of type QVector<QnUuid>. */
        LayoutBookmarksModeRole,                    /**< Role for layout's bookmarks mode state. */

        /* Item-based. */
        ItemUuidRole,                               /**< Role for item's UUID. Value of type QnUuid. */
        ItemGeometryRole,                           /**< Role for item's integer geometry. Value of type QRect. */
        ItemGeometryDeltaRole,                      /**< Role for item's floating point geometry delta. Value of type QRectF. */
        ItemCombinedGeometryRole,                   /**< Role for item's floating point combined geometry. Value of type QRectF. */
        ItemPositionRole,                           /**< Role for item's floating point position. Value of type QPointF. */
        ItemZoomRectRole,                           /**< Role for item's zoom window. Value of type QRectF. */
        ItemImageEnhancementRole,                   /**< Role for item's image enhancement params. Value of type ImageCorrectionParams. */
        ItemImageDewarpingRole,                     /**< Role for item's image dewarping params. Value of type QnItemDewarpingParams. */
        ItemFlagsRole,                              /**< Role for item's flags. Value of type int (Qn::ItemFlags). */
        ItemRotationRole,                           /**< Role for item's rotation. Value of type qreal. */
        ItemFrameDistinctionColorRole,              /**< Role for item's frame distinction color. Value of type QColor. */
        ItemFlipRole,                               /**< Role for item's flip state. Value of type bool. */
        ItemAspectRatioRole,                        /**< Role for item's aspect ratio. Value of type qreal. */
        ItemDisplayInfoRole,                        /**< Role for item's info state. Value of type bool. */

        ItemTimeRole,                               /**< Role for item's playback position, in milliseconds. Value of type qint64. Default value is -1. */
        ItemPausedRole,                             /**< Role for item's paused state. Value of type bool. */
        ItemSpeedRole,                              /**< Role for item's playback speed. Value of type qreal. */
        ItemSliderWindowRole,                       /**< Role for slider window that is displayed when the item is active. Value of type QnTimePeriod. */
        ItemSliderSelectionRole,                    /**< Role for slider selection that is displayed when the items is active. Value of type QnTimePeriod. */
        ItemCheckedButtonsRole,                     /**< Role for buttons that are checked in item's titlebar. Value of type int (QnResourceWidget::Buttons). */
        ItemDisabledButtonsRole,                    /**< Role for buttons that are not to be displayed in item's titlebar. Value of type int (QnResourceWidget::Buttons). */
        ItemHealthMonitoringButtonsRole,            /**< Role for buttons that are checked on each line of Health Monitoring widget. Value of type QnServerResourceWidget::HealthMonitoringButtons. */
        ItemVideowallReviewButtonsRole,             /**< Role for buttons that are checked on each sub-item of the videowall screen widget. Value of type QnVideowallScreenWidget::ReviewButtons. */

        ItemWidgetOptions,                          /**< Role for widget-specific options that should be set before the widget is placed on the scene. */

        /* Ptz-based. */
        PtzPresetRole,                              /**< Role for PTZ preset. Value of type QnPtzPreset. */
        PtzTourRole,                                /**< Role for PTZ tour. Value of type QnPtzTour. */
        PtzObjectIdRole,                            /**< Role for PTZ tour/preset id. Value of type QString. */
        PtzObjectNameRole,                          /**< Role for PTZ tour/preset name. Value of type QString. */
        PtzTourSpotRole,                            /**< Role for PTZ tour spot. Value of type QnPtzTourSpot. */

        /* Context-based. */
        CurrentLayoutResourceRole,
        CurrentLayoutMediaItemsRole,

        /* Arguments. */
        ActionIdRole,
        SerializedDataRole,
        ConnectionInfoRole,
        FocusElementRole,
        FocusTabRole,                               /**< Role for selecting tab in the tabbed dialogs. Value of type int. */
        TimePeriodRole,
        TimePeriodsRole,
        MergedTimePeriodsRole,
        FileNameRole,                               /**< Role for target filename. Used in TakeScreenshotAction. */
        TextRole,                                   /**< Role for generic text. Used in several places. */
        UrlRole,                                    /**< Role for target url. Used in BrowseUrlAction and QnActions::ConnectAction. */
        AutoLoginRole,                              /**< Role for flag that shows if client should connect with last credentials
                                                         (or to the last system) automatically next time */
        StorePasswordRole,                          /**< Role for flag that shows if password of successful connection should be stored.
                                                         Used in QnActions::ConnectAction. */
        CloudSystemIdRole,                          /**< Role for cloud system id (QString). Used in cloud system nodes and ConnectToCloudAction. */

        ForceRole,                                  /**< Role for 'forced' flag. Used in ConnectAction/DisconnectAction. */
        CameraBookmarkRole,                         /**< Role for the selected camera bookmark (if any). Used in Edit/RemoveCameraBookmarkAction */
        CameraBookmarkListRole,                     /**< Role for the list of bookmarks. Used in RemoveBookmarksAction */
        BookmarkTagRole,                            /**< Role for bookmark tag. Used in OpenBookmarksSearchAction */
        UuidRole,                                   /**< Role for target uuid. Used in LoadVideowallMatrixAction. */
        KeyboardModifiersRole,                      /**< Role for keyboard modifiers. Used in some Drop actions. */

        /* Others. */
        HelpTopicIdRole,                            /**< Role for item's help topic. Value of type int. */

        TranslationRole,                            /**< Role for translations. Value of type QnTranslation. */

        ItemMouseCursorRole,                        /**< Role for item's mouse cursor. */
        DisplayHtmlRole,                            /**< Same as Display role, but use HTML format. */
        DisplayHtmlHoveredRole,                     /**< Same as DisplayHtmlRole role, but used if mouse over a element */

        ModifiedRole,                               /**< Role for modified state. Value of type bool. */
        DisabledRole,                               /**< Role for disabled state. Value of type bool. */
        ValidRole,                                  /**< Role for valid state. Value of type bool. */
        ActionIsInstantRole,                        /**< Role for instant state for business rule actions. Value of type bool. */
        ShortTextRole,                              /**< Role for short text. Value of type QString. */
        PriorityRole,                               /**< Role for priority value. Value of type quint64. */

        EventTypeRole,                              /**< Role for business event type. Value of type QnBusiness::EventType. */
        EventResourcesRole,                         /**< Role for business event resources list. Value of type QSet<QnUuid>. */
        ActionTypeRole,                             /**< Role for business action type. Value of type QnBusiness::ActionType. */
        ActionResourcesRole,                        /**< Role for business action resources list. Value of type QSet<QnUuid>. */

        StorageUrlRole,                             /**< Role for storing real storage Url in storage_url_dialog. */

        IOPortDataRole,                             /**< Return QnIOPortData object. Used in IOPortDataModel */

        RecordingStatsDataRole,                     /**< Return QnCamRecordingStatsData object. Used in QnRecordingStatsModel */
        RecordingStatChartDataRole,                 /**< Return qreal for chart. Real value. Used in QnRecordingStatsModel */

        AuditRecordDataRole,                        /**< Return QnAuditRecord object */
        ColumnDataRole,                             /**< convert index col count to column enumerator */
        DecorationHoveredRole,                      /**< Same as Qt::DecorationRole but for hovered item */
        AlternateColorRole,                         /**< Use alternate color in painting */
        AuditLogChartDataRole,                      /**< Return qreal in range [0..1] for chart. Used in QnAuditLogModel */

        StorageInfoDataRole,                        /**< return QnStorageModelInfo object at QnStorageConfigWidget */
        BackupSettingsDataRole,                     /**< return BackupSettingsData, used in BackupSettings model */
        TextWidthDataRole,                          /**< used in BackupSettings model */

        ActionEmitterType,                          /** */
        ActionEmittedBy,                            /** */

        GlobalPermissionsRole,                      /**< Global permissions role. Value of type Qn::GlobalPermissions. */
        UserRoleRole,                               /**< Type of user role. Value of type Qn::UserRole. */

        RoleCount
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

    enum class ImageBehaviour
    {
        Stretch,
        Crop,
        Fit,
        Tile
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
    (Qn::ImageBehaviour),
    (metatype)(lexical)
    )

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (Qn::PaneState)(Qn::WorkbenchPane),
    (metatype)(lexical)
    )

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (Qn::LightModeFlags)(Qn::ShowOnceMessages),
    (metatype)(numeric)
    )
