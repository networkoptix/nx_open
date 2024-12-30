// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

#include <QtCore/QFlags>
#include <QtCore/QInternal>

#include <common/common_meta_types.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/vms/client/core/client_core_globals.h>

namespace Qn
{
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
    * Generic enumeration holding different data roles used in Qn classes.
    */
    enum ItemDataRole
    {
        FirstItemDataRole = nx::vms::client::core::CoreItemDataRoleCount,

        /* Tree-based. */
        NodeTypeRole,                               /**< Role for node type, see <tt>ResourceTree::NodeType</tt>. */

        /**%apidoc
         * %deprecated Vacant for another common Resource Tree role since 4.2.
         */
        ResourceTreeScopeRole,

        /* Resource-based. */
        UserResourceRole,                           /**< Role for QnUserResourcePtr. */
        VideoWallResourceRole,                      /**< Role for QnVideoWallResourcePtr */

        ResourceFlagsRole,                          /**< Role for resource flags. Value of type int (Qn::ResourceFlags). */
        ResourceIconKeyRole,                        /**< Role for resource custom icon key. Value of type int. */

        ResourceExtraStatusRole,                    /**< Custom resource status (recording, buggy, etc). Value of ResourceExtraStatus. */

        VideoWallGuidRole,                          /**< Role for videowall resource unique id. Value of type nx::Uuid. */
        VideoWallItemGuidRole,                      /**< Role for videowall item unique id. Value of type nx::Uuid. */
        VideoWallItemIndicesRole,                   /**< Role for videowall item indices list. Value of type QnVideoWallItemIndexList. */

        /** Id role for Showreel review layout. */
        ShowreelUuidRole,
        /** Item delay role for Showreel review layout. */
        ShowreelItemDelayMsRole,
        /** Item order role for Showreel review layout. */
        ShowreelItemOrderRole,
        /** Whether item swithing is manual role for Showreel review layout. */
        ShowreelIsManualRole,


        //-------------------------------------------------------------------------------------------------
        // Layout-based roles. Their numeric values must be kept intact to maintain external video
        // wall integrations. Video Wall Control protocol uses these IDs to pass messages about
        // corresponding layout item changes.

        /** Cell spacing. Value of type qreal. */
        LayoutCellSpacingRole = 276,

        /** Cell aspect ratio. Value of type qreal. */
        LayoutCellAspectRatioRole = 277,

           // Role 278 is deleted.

        /**< Minimal bounding rect. Value of type QRect. */
        LayoutMinimalBoundingRectRole = 279,

        /** Stream synchronization state. Value of type QnStreamSynchronizationState. */
        LayoutSyncStateRole = 280,

        /** 'Preview Search' layout parameters. */
        LayoutSearchStateRole = 281,

        /** Time label display. Value of type bool. */
        LayoutTimeLabelsRole = 282,

        /** Overriden layout's permissions. Value of type int (Qn::Permissions). */
        LayoutPermissionsRole = 283,

        /** Selected items. Value of type QVector<nx::Uuid>. */
        LayoutSelectionRole = 284,

        /** Active item. Value of type nx::Uuid. */
        LayoutActiveItemRole = 285,

        /** Watermark info (when loaded from file). */
        LayoutWatermarkRole = 286,

//-------------------------------------------------------------------------------------------------
        // Item-based roles. Their numeric values must be kept intact to maintain external video
        // wall integrations. Video Wall Control protocol uses these IDs to pass messages about
        // corresponding layout item changes.

        /** Id. Value of type nx::Uuid. */
        ItemUuidRole = 287,

        /** Integer geometry. Value of type QRect. */
        ItemGeometryRole = 288,

        /** Floating point geometry delta. Value of type QRectF. */
        ItemGeometryDeltaRole = 289,

        /** Floating point combined geometry. Value of type QRectF. */
        ItemCombinedGeometryRole = 290,

        /** Floating point position. Value of type QPointF. */
        ItemPositionRole = 291,

        /**
         * Flag which controls whether zoom window rectangle should be visible for the
         * corresponding zoom window. Value of type bool.
         */
        ItemZoomWindowRectangleVisibleRole = 293,

        /** Image enhancement params. Value of type nx::vms::api::ImageCorrectionData. */
        ItemImageEnhancementRole = 294,

        /** Image dewarping params. Value of type nx::vms::api::dewarping::ViewData. */
        ItemImageDewarpingRole = 295,

        /** Flags. Value of type int (Qn::ItemFlags). */
        ItemFlagsRole = 296,

        /** Rotation. Value of type qreal. */
        ItemRotationRole = 297,

        /** Frame distinction color. Value of type QColor. */
        ItemFrameDistinctionColorRole = 298,

        /** Flip state. Value of type bool. */
        ItemFlipRole = 299,

        /** Aspect ratio. Value of type qreal. */
        ItemAspectRatioRole,

        /** Info state. Value of type bool. */
        ItemDisplayInfoRole,

        /** PTZ control state. Value of type bool. */
        ItemPtzControlRole,

        /** Display analytics objects state. Value of type bool. */
        ItemDisplayAnalyticsObjectsRole,

        /** Display ROI state. Value of type bool. */
        ItemDisplayRoiRole,

        /** Camera hotspots display state. Value of type bool. */
        ItemDisplayHotspotsRole,

        /** Placeholder pixmap. Value of type QPixmap. */
        ItemPlaceholderRole,

        /** Playback position, in milliseconds. Value of type qint64. Default value is -1. */
        ItemTimeRole,

        /** Paused state. Value of type bool. */
        ItemPausedRole,

        /** Playback speed. Value of type qreal. */
        ItemSpeedRole,

        /** Slider window that is displayed when the item is active. Value of type QnTimePeriod. */
        ItemSliderWindowRole,

        /**
         * Slider selection that is displayed when the items is active. Value of type QnTimePeriod.
         */
        ItemSliderSelectionRole,

        /** Buttons that are checked in item's titlebar. Value of type int (Qn::WidgetButtons). */
        ItemCheckedButtonsRole,

        /**
         * Buttons that are not to be displayed in item's titlebar. Value of type
         * int (Qn::WidgetButtons).
         */
        ItemDisabledButtonsRole,

        /**
         * Buttons that are checked on each line of Health Monitoring widget. Value of type
         * QnServerResourceWidget::HealthMonitoringButtons.
         */
        ItemHealthMonitoringButtonsRole,

        /** Motion region selection. Value of type MotionSelection. */
        ItemMotionSelectionRole,

        /** Analytics region selection. Value of type QRectF. */
        ItemAnalyticsSelectionRole,

        /** Saved web page state when user switches the layout. */
        ItemWebPageSavedStateDataRole,

        /** Skipping 'focused' state on addition. */
        ItemSkipFocusOnAdditionRole,

        /** Add item or as already zoomed or switch to existing item in zoomed state. */
        ItemAddInZoomedStateRole,

//-------------------------------------------------------------------------------------------------
        /* Ptz-based. */
        PtzPresetRole,                              /**< Role for PTZ preset. Value of type QnPtzPreset. */
        PtzTourRole,                                /**< Role for PTZ tour. Value of type QnPtzTour. */
        PtzObjectIdRole,                            /**< Role for PTZ tour/preset id. Value of type QString. */
        PtzObjectNameRole,                          /**< Role for PTZ tour/preset name. Value of type QString. */
        PtzTourSpotRole,                            /**< Role for PTZ tour spot. Value of type QnPtzTourSpot. */
        PtzSpeedRole,                               /**< Role for PTZ speed. Value of type QVector3D. */
        PtzPresetIndexRole,                         /**< Role for PTZ preset index. Value of type int. */

        /* Context-based. */
        CurrentLayoutResourceRole,
        CurrentLayoutMediaItemsRole,

        /**
          * Special layout roles
          */

        IsSpecialLayoutRole,                        /**< Role for showreel review layouts. */
        LayoutIconRole,
        LayoutFlagsRole,                            /**< Role for additional QnLayoutFlags value. */
        LayoutMarginsRole,                          /**< Role for additional QMargins (in pixels). */
        CustomPanelTitleRole,
        CustomPanelDescriptionRole,
        CustomPanelActionsRole,

        ActionIdRole,

        /** Role for command-line startup parameters. */
        StartupParametersRole,

        ConnectionInfoRole,
        FocusElementRole,
        FocusTabRole,                               /**< Role for selecting tab in the tabbed dialogs. Value of type int. */
        TimePeriodRole,
        TimePeriodsRole,
        MergedTimePeriodsRole,
        FileNameRole,                               /**< Role for target filename. Used in TakeScreenshotAction. */
        TextRole,                                   /**< Role for generic text. Used in several places. */
        IntRole,                                    /**< Role for generic integer. Used in several places. */

        /** Role for target url. Used in BrowseUrlAction. */
        UrlRole,

        /** LogonData structure. Used in the ConnectAction and SetupFactoryServerAction. */
        LogonDataRole,

        /**
         * Role for cloud system id (QString). Used in cloud system nodes in the resources tree and
         * as the ConnectToCloudAction parameter.
         */
        CloudSystemIdRole,

        /**
         * Role for local system id (QString). Used in system tabbar.
         */
        LocalSystemIdRole,

        /** Remote session ID. Used in OpenSessionInNewWindowAction. */
        SessionIdRole,

        /** CloudSystemConnectData structure. Used in the ConnectToCloudSystemAction. */
        CloudSystemConnectDataRole,

        /** Role for 'forced' flag. Used in DisconnectAction. */
        ForceRole,

        CameraBookmarkListRole,                     /**< Role for the list of bookmarks. Used in RemoveBookmarksAction */
        KeyboardModifiersRole,                      /**< Role for keyboard modifiers. Used in some Drop actions. */

        /* Others. */
        HelpTopicIdRole,                            /**< Role for item's help topic. Value of type int. */

        ItemMouseCursorRole,                        /**< Role for item's mouse cursor. */
        DisplayHtmlRole,                            /**< Same as Display role, but use HTML format. */
        DisplayHtmlHoveredRole,                     /**< Same as DisplayHtmlRole role, but used if mouse over a element */

        ModifiedRole,                               /**< Role for modified state. Value of type bool. */
        DisabledRole,                               /**< Role for disabled state. Value of type bool. */
        ValidRole,                                  /**< Role for valid state. Value of type bool. */

        ShortTextRole,                              /**< Role for short text. Value of type QString. */
        PriorityRole,                               /**< Role for priority value. Value of type quint64. */

        /** Role for an event type. Value of type QString. */
        EventTypeRole,

        /** Role for an action type. Value of type QString. */
        ActionTypeRole,

        /** Role for an action. Value of type nx::vms::rules::ActionPtr. */
        ActionDataRole,

        LookupListEntryRole,                        /**< Role for value of Lookup List Entry object (nx::vms::api::LookupListEntry) */

        StorageUrlRole,                             /**< Role for storing real storage Url in storage_url_dialog. */

        IOPortDataRole,                             /**< Return QnIOPortData object. Used in IOPortDataModel */

        AuditRecordDataRole,                        /**< Return QnLegacyAuditRecord object */
        ColumnDataRole,                             /**< convert index col count to column enumerator */
        DecorationHoveredRole,                      /**< Same as Qt::DecorationRole but for hovered item */
        AlternateColorRole,                         /**< Use alternate color in painting */
        AuditLogChartDataRole,                      /**< Return qreal in range [0..1] for chart. Used in QnAuditLogModel */

        StorageInfoDataRole,                        /**< return QnStorageModelInfo object at QnStorageConfigWidget */
        BackupSettingsDataRole,                     /**< return BackupSettingsData, used in BackupSettings model */
        TextWidthDataRole,                          /**< used in BackupSettings model */

        ActionEmitterType,                          /** */
        ActionEmittedBy,                            /** */

        GlobalPermissionsRole,                      /**< Global permissions role. Value of type GlobalPermissions. */

        ValidationStateRole,                        /**< A role for validation state. Value of type QValidator::State. */
        ResolutionModeRole,                         /**< Role for resolution mode. */

        ForceShowCamerasList,                       /**< Used for default password dialog. */
        ParentWidgetRole,                           /**< Used for dialog's parent widget. */

        AdditionalTextRole,                         /**< Role for additional description text (QString). */
        RemovableRole,                              /**< An item is removable (bool). */
        CommandActionRole,                          /**< Command action (QSharedPointer<QAction>). */
        AdditionalActionRole,                       /**< Additional action (QSharedPointer<QAction>). */
        OnCloseActionRole,                          /**< Action to trigger on tile close (QSharedPointer<QAction>). */
        TimeoutRole,                                /**< Role for timeout or lifetime in milliseconds (std::chrono::milliseconds). */
        BusyIndicatorVisibleRole,                   /**< Role for toggling busy indicator (bool). */
        ProgressValueRole,                          /**< Role for specifying progress value [0..1] (float). */
        ProgressFormatRole,                         /**< Role for specifying progress format, when null default is used: "%p%" (QString). */
        NotificationLevelRole,                      /**< Role for notification level (QnNotificationLevel::Value). */
        CreateContextMenuRole,                      /**< Role for creating context menu (QSharedPointer<QMenu>). */
        ForcePreviewLoaderRole,                     /**< Display loading indicator on tile preview. */
        ShowVideoPreviewRole,                       /**< Display camera video stream on tile preview. */

        SelectOnOpeningRole,                        /**< Role for single-selecting an item (or first of multiple items) added to current layout (bool). */

        ExtraInfoRole,                              /**< Role for extra resource information in the tree (QString). */
        ForceExtraInfoRole,                         /**< Role for forcing extra resource information in the tree (bool). */

        AutoExpandRole,                             /**< Role for automatic expanding of tree nodes on first show (bool). */

        RawResourceRole,                            /**< As ResourceRole, but with QnResource* for QML. */

        /** Enable advanced UI features (typically in settings dialogs). */
        AdvancedModeRole,

        // Additional roles for the resource tree. Placed at the end of the list for purpose
        // not to change enumeration values for BlueBox integration.
        ParentResourceRole,                         /**< Role for parent resource if such exists (QnResourcePtr). */
        CameraGroupIdRole,                          /**< Role for camera group ID, related to recorders and multisensor cameras (QString). */
        ResourceTreeCustomGroupIdRole,              /**< Role for user defined group ID, used for the custom cameras grouping within Resource Tree (QString). */

        ParentNodeTypeRole,                         /**< Role for parent node type in action parameters related to the resource tree (ResourceTree::NodeType). */
        TopLevelParentNodeTypeRole,                 /**< Role for topmost parent node type in action parameters related to the resource tree (ResourceTree::NodeType). */
        TargetResourceTreeCustomGroupIdRole,        /**< Role used to access parameter for Resource Tree drag & drop interactions (QString). */
        OnlyResourceTreeSiblingsRole,               /**< Role indicating whether only sibling entities are passed to the action (bool). */
        SelectedGroupIdsRole,                       /**< Role containing all currently selected group IDs (QStringList). */
        SelectedGroupFullyMatchesFilter,            /**< Role indicating whether selected group fully matches current search filter, always true for empty search string. (bool). */

        AnalyticsEngineNameRole,                    /**< Role for related analytics engine name. (QString) */
        AnalyticsObjectTypeIdRole,                  /**< Role for type Id of Anlytics Object. (QString) */

        /**
         * Role for passing existing remote connection.
         * Type: `nx::vms::client::core::RemoteConnectionPtr`.
         */
        RemoteConnectionRole,
        MaximumTooltipSizeRole, /**< Role for passing maximum size of the tooltip (QSize) */
        SystemDescriptionRole, /**< Role for system description (SystemDescriptionPtr) */

        SortKeyRole, /**< Role for text sort key (QString). */
        FilterKeyRole, /**< Role for text filter key (QString). */

        /**
         * Role for node-controlled flattening control in the resource tree (bool).
         * See FlatteningGroupEntity for the details.
         */
        FlattenedRole,
        WorkbenchStateRole,

        ItemDataRoleCount,
    };

    std::string toString(ItemDataRole value);
    bool fromString(const std::string_view& str, ItemDataRole* value);

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

    enum MarginType
    {
        PanelMargins = 0x1,     //< Margins from panels, e.g. resource tree or timeline.
        LayoutMargins = 0x2,    //< Margins from layout, e.g. tour review.

        CombinedMargins = PanelMargins | LayoutMargins
    };
    Q_DECLARE_FLAGS(MarginTypes, MarginType)
    Q_DECLARE_OPERATORS_FOR_FLAGS(MarginTypes)

    enum class CellSpacing
    {
        None,
        Small,
        Medium,
        Large
    };

    /**
     * Time display mode.
     */
    NX_REFLECTION_ENUM(TimeMode,
        ServerTimeMode,
        ClientTimeMode
    )

    /**
     * Columns in the resource tree model.
     */
    enum ResourceTreeColumn
    {
        NameColumn,     /**< Resource icon and name. */
        ColumnCount
    };

    /**
     * Overlay for resource widgets.
     */
    enum ResourceStatusOverlay
    {
        EmptyOverlay,
        LoadingOverlay,
        NoDataOverlay,
        UnauthorizedOverlay,
        AccessDeniedOverlay,
        NoExportPermissionOverlay,
        OfflineOverlay,
        AnalogWithoutLicenseOverlay,
        VideowallWithoutLicenseOverlay,
        ServerOfflineOverlay,
        ServerUnauthorizedOverlay,
        IoModuleDisabledOverlay,
        TooManyOpenedConnectionsOverlay,
        PasswordRequiredOverlay,
        NoLiveStreamOverlay,
        OldFirmwareOverlay,
        CannotDecryptMediaOverlay,
        MismatchedCertificateOverlay,
        InformationRequiredOverlay,
        SaasShutDown,
        RestrictedOverlay,

        OverlayCount
    };

    enum class ResourceOverlayButton
    {
        Empty,
        Diagnostics,
        EnableLicense,
        MoreLicenses,
        Settings,
        SetPassword,
        UnlockEncryptedArchive,
        Authorize,

        /** Provides information about required actions. */
        RequestInformation
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

        LightModeACS = LightModeSmallWindow
            | LightModeNoSceneBackground
            | LightModeNoNotifications
            | LightModeNoShadows
            | LightModeNoNewWindow
            | LightModeNoLayoutBackground
            | LightModeNoZoomWindows,

        LightModeVideoWall          = LightModeNoSceneBackground | LightModeNoNotifications | LightModeNoShadows,
        LightModeFull               = 0x7FFFFFFF

    };
    Q_DECLARE_FLAGS(LightModeFlags, LightModeFlag)
    Q_DECLARE_OPERATORS_FOR_FLAGS(LightModeFlags)

    NX_REFLECTION_ENUM_CLASS(ImageBehavior,
        Stretch,
        Crop,
        Fit,
        Tile
    )

    /** Workbench pane state. */
    NX_REFLECTION_ENUM_CLASS(PaneState,
        Unpinned, /**< A pane is unpinned and closed. The timeline is unpinned and opened. */
        Opened, /**< A pane is pinned and opened. */
        Closed /**< A pane is pinned and closed. The timeline is unpinned and closed. */
    )

    /** Workbench pane ID. */
    NX_REFLECTION_ENUM_CLASS(WorkbenchPane,
        Title, /**< Title pane. */
        Tree, /**< Resource tree pane. */
        Notifications, /**< Notifications pane. */
        Navigation, /**< Navigation pane. */
        Calendar, /**< Calendar pane. */
        Thumbnails, /**< Thumbnails pane. */
        SpecialLayout /**< Special layout pane. */
    )

    enum class ButtonAccent
    {
        NoAccent,
        Standard,
        Warning
    };

} // namespace Qn
