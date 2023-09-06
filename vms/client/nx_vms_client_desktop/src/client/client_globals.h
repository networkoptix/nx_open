// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

#include <QtCore/QFlags>
#include <QtCore/QInternal>

#include <common/common_meta_types.h>
#include <nx/reflect/enum_instrument.h>

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
        FirstItemDataRole = Qt::UserRole,

        /* Tree-based. */
        NodeTypeRole,                               /**< Role for node type, see <tt>ResourceTree::NodeType</tt>. */

        /**%apidoc
         * %deprecated Vacant for another common Resource Tree role since 4.2.
         */
        ResourceTreeScopeRole,

        /* Resource-based. */
        ResourceRole,                               /**< Role for QnResourcePtr. */
        UserResourceRole,                           /**< Role for QnUserResourcePtr. */
        LayoutResourceRole,                         /**< Role for QnLayoutResourcePtr. */
        MediaServerResourceRole,                    /**< Role for QnMediaServerResourcePtr. */
        VideoWallResourceRole,                      /**< Role for QnVideoWallResourcePtr */

        ResourceNameRole,                           /**< Role for resource name. Value of type QString. */
        ResourceFlagsRole,                          /**< Role for resource flags. Value of type int (Qn::ResourceFlags). */
        ResourceStatusRole,                         /**< Role for resource status. Value of type int (nx::vms::api::ResourceStatus). */
        ResourceIconKeyRole,                        /**< Role for resource custom icon key. Value of type int. */

        ResourceExtraStatusRole,                    /**< Custom resource status (recording, buggy, etc). Value of ResourceExtraStatus. */

        VideoWallGuidRole,                          /**< Role for videowall resource unique id. Value of type QnUuid. */
        VideoWallItemGuidRole,                      /**< Role for videowall item unique id. Value of type QnUuid. */
        VideoWallItemIndicesRole,                   /**< Role for videowall item indices list. Value of type QnVideoWallItemIndexList. */

        /** Id role for Showreel review layout. */
        ShowreelUuidRole,
        /** Item delay role for Showreel review layout. */
        ShowreelItemDelayMsRole,
        /** Item order role for Showreel review layout. */
        ShowreelItemOrderRole,
        /** Whether item swithing is manual role for Showreel review layout. */
        ShowreelIsManualRole,

        /* Layout-based. */
        LayoutCellSpacingRole,                      /**< Role for layout's cell spacing. Value of type qreal. */
        LayoutCellAspectRatioRole,                  /**< Role for layout's cell aspect ratio. Value of type qreal. */
        LayoutMinimalBoundingRectRole = 279,        /**< Role for layout's minimal bounding rect. Value of type QRect. */
        LayoutSyncStateRole,                        /**< Role for layout's stream synchronization state. Value of type QnStreamSynchronizationState. */
        LayoutSearchStateRole,                      /**< Role for 'Preview Search' layout parameters. */
        LayoutTimeLabelsRole,                       /**< Role for layout's time label display. Value of type bool. */
        LayoutPermissionsRole,                      /**< Role for overriding layout's permissions. Value of type int (Qn::Permissions). */
        LayoutSelectionRole,                        /**< Role for layout's selected items. Value of type QVector<QnUuid>. */
        LayoutActiveItemRole,                       /**< Role for layout active item. Value of type QnUuid. */
        LayoutWatermarkRole,                        /**< Role for layout watermark (when loaded from file). */

        /* Item-based. */
        ItemUuidRole,                               /**< Role for item's UUID. Value of type QnUuid. */
        ItemGeometryRole,                           /**< Role for item's integer geometry. Value of type QRect. */
        ItemGeometryDeltaRole,                      /**< Role for item's floating point geometry delta. Value of type QRectF. */
        ItemCombinedGeometryRole,                   /**< Role for item's floating point combined geometry. Value of type QRectF. */
        ItemPositionRole,                           /**< Role for item's floating point position. Value of type QPointF. */
        ItemZoomRectRole,                           /**< Role for item's zoom window. Value of type QRectF. */
        ItemZoomWindowRectangleVisibleRole,         /**< Role for item's flag which controls if zoom window rectangle should be visible for the corresponding zoom window. */
        ItemImageEnhancementRole,                   /**< Role for item's image enhancement params. Value of type nx::vms::api::ImageCorrectionData. */
        ItemImageDewarpingRole,                     /**< Role for item's image dewarping params. Value of type nx::vms::api::dewarping::ViewData. */
        ItemFlagsRole,                              /**< Role for item's flags. Value of type int (Qn::ItemFlags). */
        ItemRotationRole,                           /**< Role for item's rotation. Value of type qreal. */
        ItemFrameDistinctionColorRole,              /**< Role for item's frame distinction color. Value of type QColor. */
        ItemFlipRole,                               /**< Role for item's flip state. Value of type bool. */
        ItemAspectRatioRole,                        /**< Role for item's aspect ratio. Value of type qreal. */
        ItemDisplayInfoRole,                        /**< Role for item's info state. Value of type bool. */
        ItemPtzControlRole,                         /**< Role for item's PTZ control state. Value of type bool. */
        ItemDisplayAnalyticsObjectsRole,            /**< Role for item display analytics objects state. Value of type bool. */
        ItemDisplayRoiRole,                         /**< Role for item display ROI state. Value of type bool. */
        ItemPlaceholderRole,                        /**< Role for item's placeholder pixmap. Value of type QPixmap. */
        ItemDisplayHotspotsRole,                    /**< Role for item's camera hotspots display state. Value of type bool. */

        ItemTimeRole,                               /**< Role for item's playback position, in milliseconds. Value of type qint64. Default value is -1. */
        ItemPausedRole,                             /**< Role for item's paused state. Value of type bool. */
        ItemSpeedRole,                              /**< Role for item's playback speed. Value of type qreal. */
        ItemSliderWindowRole,                       /**< Role for slider window that is displayed when the item is active. Value of type QnTimePeriod. */
        ItemSliderSelectionRole,                    /**< Role for slider selection that is displayed when the items is active. Value of type QnTimePeriod. */
        ItemCheckedButtonsRole,                     /**< Role for buttons that are checked in item's titlebar. Value of type int (Qn::WidgetButtons). */
        ItemDisabledButtonsRole,                    /**< Role for buttons that are not to be displayed in item's titlebar. Value of type int (Qn::WidgetButtons). */
        ItemHealthMonitoringButtonsRole,            /**< Role for buttons that are checked on each line of Health Monitoring widget. Value of type QnServerResourceWidget::HealthMonitoringButtons. */
        ItemMotionSelectionRole,                    /**< Role for motion region selection. Value of type MotionSelection. */
        ItemAnalyticsSelectionRole,                 /**< Role for analytics region selection. Value of type QRectF. */
        ItemWebPageSavedStateDataRole,              /**< Role for saved web page state when user switches the layout. */
        ItemSkipFocusOnAdditionRole,                /**< Role for skipping 'focused' state on addition. */

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

        /** CloudSystemConnectData structure. Used in the ConnectToCloudSystemAction. */
        CloudSystemConnectDataRole,

        /** Role for 'forced' flag. Used in DisconnectAction. */
        ForceRole,

        CameraBookmarkRole,                         /**< Role for the selected camera bookmark (if any). Used in Edit/RemoveBookmarkAction */
        CameraBookmarkListRole,                     /**< Role for the list of bookmarks. Used in RemoveBookmarksAction */
        BookmarkTagRole,                            /**< Role for bookmark tag. Used in OpenBookmarksSearchAction */
        UuidRole,                                   /**< Role for target uuid. Used in LoadVideowallMatrixAction. */
        KeyboardModifiersRole,                      /**< Role for keyboard modifiers. Used in some Drop actions. */

        /* Others. */
        HelpTopicIdRole,                            /**< Role for item's help topic. Value of type int. */

        ItemMouseCursorRole,                        /**< Role for item's mouse cursor. */
        DisplayHtmlRole,                            /**< Same as Display role, but use HTML format. */
        DisplayHtmlHoveredRole,                     /**< Same as DisplayHtmlRole role, but used if mouse over a element */

        ModifiedRole,                               /**< Role for modified state. Value of type bool. */
        DisabledRole,                               /**< Role for disabled state. Value of type bool. */
        ValidRole,                                  /**< Role for valid state. Value of type bool. */
        ActionIsInstantRole,                        /**< Role for instant state for business rule actions. Value of type bool. */
        ShortTextRole,                              /**< Role for short text. Value of type QString. */
        PriorityRole,                               /**< Role for priority value. Value of type quint64. */

        EventTypeRole,                              /**< Role for business event type. Value of type nx::vms::api::EventType. */
        EventResourcesRole,                         /**< Role for business event resources list. Value of type QSet<QnUuid>. */
        ActionTypeRole,                             /**< Role for business action type. Value of type nx::vms::api::ActionType. */
        ActionResourcesRole,                        /**< Role for business action resources list. Value of type QSet<QnUuid>. */
        ActionDataRole,                             /**< Role for business action. Value of type vms::event::AbstractActionPtr. */
        RuleModelRole, /* #deprecate #3.2 */        /**< Role for business rule caching model. Value of type QnBusinessRuleViewModelPtr. */
        EventParametersRole,                            /**< Role for business event parameters. Value of type nx::vms::event::EventParameters. */

        StorageUrlRole,                             /**< Role for storing real storage Url in storage_url_dialog. */

        IOPortDataRole,                             /**< Return QnIOPortData object. Used in IOPortDataModel */

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

        GlobalPermissionsRole,                      /**< Global permissions role. Value of type GlobalPermissions. */

        ValidationStateRole,                        /**< A role for validation state. Value of type QValidator::State. */
        ResolutionModeRole,                         /**< Role for resolution mode. */

        ForceShowCamerasList,                       /**< Used for default password dialog. */
        ParentWidgetRole,                           /**< Used for dialog's parent widget. */

        TimestampRole,                              /**< Role for timestamp in microseconds since epoch (std::chrono::microseconds). */
        TimestampTextRole,                          /**< Role for timestamp text (QString). */
        DescriptionTextRole,                        /**< Role for generic description text (QString). */
        AdditionalTextRole,                         /**< Role for additional description text (QString). */
        AnalyticsAttributesRole,                    /**< Role for analytics attribute lists (QList<nx::vms::client::desktop::analytics::Attribute>). */
        RemovableRole,                              /**< An item is removable (bool). */
        CommandActionRole,                          /**< Command action (QSharedPointer<QAction>). */
        AdditionalActionRole,                       /**< Additional action (QSharedPointer<QAction>). */
        ResourceListRole,                           /**< Resource list (QnResourceList). */
        DisplayedResourceListRole,                  /**< Resource list displayed in a Right Panel tile (QnResourceList or QStringList). */
        PreviewTimeRole,                            /**< Role for camera preview time in microseconds since epoch (std::chrono::microseconds). */
        TimeoutRole,                                /**< Role for timeout or lifetime in milliseconds (std::chrono::milliseconds). */
        BusyIndicatorVisibleRole,                   /**< Role for toggling busy indicator (bool). */
        ProgressValueRole,                          /**< Role for specifying progress value [0..1] (float). */
        ProgressFormatRole,                         /**< Role for specifying progress format, when null default is used: "%p%" (QString). */
        DurationRole,                               /**< Role for duration in microseconds (std::chrono::microseconds). */
        NotificationLevelRole,                      /**< Role for notification level (QnNotificationLevel::Value). */
        CreateContextMenuRole,                      /**< Role for creating context menu (QSharedPointer<QMenu>). */
        ForcePrecisePreviewRole,                    /**< Role for forcing precise preview frame (bool). */
        PreviewStreamSelectionRole,                 /**< Role for camera preview stream (ImageRequest::StreamSelectionMode). */
        ObjectTrackIdRole,                          /**< Role for camera preview stream (CameraImageRequest::objectTrackId). */
        HasExternalBestShotRole,                    /**< Whether object detection track has an external best shot image (bool). */
        ForcePreviewLoaderRole,                     /**< Display loading indicator on tile preview. */

        DecorationPathRole,                         /**< Role for icon path (QString). */

        SelectOnOpeningRole,                        /**< Role for single-selecting an item (or first of multiple items) added to current layout (bool). */

        ExtraInfoRole,                              /**< Role for extra resource information in the tree (QString). */
        ForceExtraInfoRole,                         /**< Role for forcing extra resource information in the tree (bool). */

        AutoExpandRole,                             /**< Role for automatic expanding of tree nodes on first show (bool). */

        RawResourceRole,                            /**< As ResourceRole, but with QnResource* for QML. */

        /** Enable advanced UI features (typically in settings dialogs). */
        AdvancedModeRole,

        // Model notification roles. Do not necessarily pass any data but implement
        // item-related view-to-model notifications via setData which can be proxied.
        DefaultNotificationRole,                    /**< Role to perform default item action (no data). */
        ActivateLinkRole,                           /**< Role to parse and follow hyperlink (QString). */

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

        /**
         * Role for passing existing remote connection.
         * Type: `nx::vms::client::core::RemoteConnectionPtr`.
         */
        RemoteConnectionRole,
        MaximumTooltipSizeRole, /**< Role for passing maximum size of the tooltip (QSize) */
        SystemDescriptionRole, /**< Role for system description (QnSystemDescriptionPtr) */

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
        NoVideoDataOverlay,
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
        SaasSuspended,
        SaasShutDown,

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

    NX_REFLECTION_ENUM_CLASS(ThumbnailStatus,
        Invalid,
        Loading,
        Loaded,
        NoData,
        Refreshing
    )

    enum class ButtonAccent
    {
        NoAccent,
        Standard,
        Warning
    };


} // namespace Qn
