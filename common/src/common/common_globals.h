#pragma once

#define BOOST_BIND_NO_PLACEHOLDERS
#include <cassert>
#include <limits>

#include <QtCore/QtGlobal>
#include <QtCore/QMetaType>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <utils/common/unused.h>
#include <utils/common/model_functions_fwd.h>

#ifdef THIS_BLOCK_IS_REQUIRED_TO_MAKE_FILE_BE_PROCESSED_BY_MOC_DO_NOT_DELETE
Q_OBJECT
#endif
QN_DECLARE_METAOBJECT_HEADER(Qn,
    Border Corner ExtrapolationMode CameraCapability PtzObjectType PtzCommand PtzDataField PtzCoordinateSpace
    PtzCapability StreamFpsSharingMethod MotionType TimePeriodType TimePeriodContent SystemComponent ItemDataRole
    ConnectionRole ResourceStatus BitratePerGopType
    StreamQuality SecondStreamQuality PanicMode RebuildState BackupState RecordingType PropertyDataType SerializationFormat PeerType StatisticsDeviceType
    ServerFlag BackupType CameraBackupQuality CameraStatusFlag IOPortType IODefaultState AuditRecordType AuthResult
    RebuildAction BackupAction FailoverPriority
    Permission GlobalPermission
    ,
    Borders Corners ResourceFlags CameraCapabilities PtzDataFields PtzCapabilities PtzTraits
    MotionTypes TimePeriodTypes
    ServerFlags CameraBackupQualities TimeFlags CameraStatusFlags IOPortTypes
    Permissions GlobalPermissions
    )


    // TODO: #Elric #5.0 use Qt::Edge ?
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
    Q_DECLARE_FLAGS(Borders, Border)
    Q_DECLARE_OPERATORS_FOR_FLAGS(Borders)


    /**
     * Generic enumeration describing corners of a rectangle.
     */
    enum Corner {
        NoCorner = 0,
        TopLeftCorner = 0x1,
        TopRightCorner = 0x2,
        BottomLeftCorner = 0x4,
        BottomRightCorner = 0x8,
        AllCorners = TopLeftCorner | TopRightCorner | BottomLeftCorner | BottomRightCorner
    };
    Q_DECLARE_FLAGS(Corners, Corner)
    Q_DECLARE_OPERATORS_FOR_FLAGS(Corners)

    enum ExtrapolationMode {
        ConstantExtrapolation,
        LinearExtrapolation,
        PeriodicExtrapolation
    };

    enum CameraCapability {
        NoCapabilities                      = 0x000,
        PrimaryStreamSoftMotionCapability   = 0x004,
        RelayInputCapability                = 0x008,
        RelayOutputCapability               = 0x010,
        ShareIpCapability                   = 0x020,
        AudioTransmitCapability             = 0x040
    };
    Q_DECLARE_FLAGS(CameraCapabilities, CameraCapability)
    Q_DECLARE_OPERATORS_FOR_FLAGS(CameraCapabilities)

    enum PtzCommand {
        ContinuousMovePtzCommand,
        ContinuousFocusPtzCommand,
        AbsoluteDeviceMovePtzCommand,
        AbsoluteLogicalMovePtzCommand,
        ViewportMovePtzCommand,

        GetDevicePositionPtzCommand,
        GetLogicalPositionPtzCommand,
        GetDeviceLimitsPtzCommand,
        GetLogicalLimitsPtzCommand,
        GetFlipPtzCommand,

        CreatePresetPtzCommand,
        UpdatePresetPtzCommand,
        RemovePresetPtzCommand,
        ActivatePresetPtzCommand,
        GetPresetsPtzCommand,

        CreateTourPtzCommand,
        RemoveTourPtzCommand,
        ActivateTourPtzCommand,
        GetToursPtzCommand,

        GetActiveObjectPtzCommand,
        UpdateHomeObjectPtzCommand,
        GetHomeObjectPtzCommand,

        GetAuxilaryTraitsPtzCommand,
        RunAuxilaryCommandPtzCommand,

        GetDataPtzCommand,

        InvalidPtzCommand = -1
    };

    enum PtzDataField {
        CapabilitiesPtzField    = 0x001,
        DevicePositionPtzField  = 0x002,
        LogicalPositionPtzField = 0x004,
        DeviceLimitsPtzField    = 0x008,
        LogicalLimitsPtzField   = 0x010,
        FlipPtzField            = 0x020,
        PresetsPtzField         = 0x040,
        ToursPtzField           = 0x080,
        ActiveObjectPtzField    = 0x100,
        HomeObjectPtzField      = 0x200,
        AuxilaryTraitsPtzField  = 0x400,
        NoPtzFields             = 0x000,
        AllPtzFields            = 0xFFF
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(PtzDataField)

    Q_DECLARE_FLAGS(PtzDataFields, PtzDataField)
    Q_DECLARE_OPERATORS_FOR_FLAGS(PtzDataFields)

    enum RebuildState {
        RebuildState_None        = 1,
        RebuildState_FullScan    = 2,
        RebuildState_PartialScan = 3
        //RebuildState_Canceled    = 4,
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(RebuildState)

    enum BackupState {
        BackupState_None        = 0,
        BackupState_InProgress  = 1
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(BackupState)

    enum PtzCoordinateSpace {
        DevicePtzCoordinateSpace,
        LogicalPtzCoordinateSpace
    };

    enum PtzObjectType {
        PresetPtzObject,
        TourPtzObject,

        InvalidPtzObject = -1
    };

    enum PtzCapability {
        NoPtzCapabilities                   = 0x00000000,

        ContinuousPanCapability             = 0x00000001,
        ContinuousTiltCapability            = 0x00000002,
        ContinuousZoomCapability            = 0x00000004,
        ContinuousFocusCapability           = 0x00000008,

        AbsolutePanCapability               = 0x00000010,
        AbsoluteTiltCapability              = 0x00000020,
        AbsoluteZoomCapability              = 0x00000040,

        ViewportPtzCapability               = 0x00000080,

        FlipPtzCapability                   = 0x00000100,
        LimitsPtzCapability                 = 0x00000200,

        DevicePositioningPtzCapability      = 0x00001000,
        LogicalPositioningPtzCapability     = 0x00002000,

        PresetsPtzCapability                = 0x00010000,
        ToursPtzCapability                  = 0x00020000,
        ActivityPtzCapability               = 0x00040000,
        HomePtzCapability                   = 0x00080000,

        AsynchronousPtzCapability           = 0x00100000,
        SynchronizedPtzCapability           = 0x00200000,
        VirtualPtzCapability                = 0x00400000,

        AuxilaryPtzCapability               = 0x01000000,

        /* Shortcuts */
        ContinuousPanTiltCapabilities       = ContinuousPanCapability | ContinuousTiltCapability,
        ContinuousPtzCapabilities           = ContinuousPanCapability | ContinuousTiltCapability | ContinuousZoomCapability,
        AbsolutePtzCapabilities             = AbsolutePanCapability | AbsoluteTiltCapability | AbsoluteZoomCapability,
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(PtzCapability)

    Q_DECLARE_FLAGS(PtzCapabilities, PtzCapability)
    Q_DECLARE_OPERATORS_FOR_FLAGS(PtzCapabilities)


    enum Projection {
        RectilinearProjection,
        EquirectangularProjection
    };


    enum PtzTrait {
        NoPtzTraits             = 0x00,
        FourWayPtzTrait         = 0x01,
        EightWayPtzTrait        = 0x02,
        ManualAutoFocusPtzTrait = 0x04,
    };
    Q_DECLARE_FLAGS(PtzTraits, PtzTrait);
    Q_DECLARE_OPERATORS_FOR_FLAGS(PtzTraits);


    enum StreamFpsSharingMethod {
        /** If second stream is running whatever fps it has, first stream can get maximumFps - secondstreamFps. */
        BasicFpsSharing,

        /** If second stream is running whatever megapixel it has, first stream can get maxMegapixels - secondstreamPixels. */
        PixelsFpsSharing,

        /** Second stream does not affect first stream's fps. */
        NoFpsSharing
    };


    enum MotionType {
        MT_Default      = 0x0,
        MT_HardwareGrid = 0x1,
        MT_SoftwareGrid = 0x2,
        MT_MotionWindow = 0x4,
        MT_NoMotion     = 0x8
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(MotionType)

    Q_DECLARE_FLAGS(MotionTypes, MotionType)
    Q_DECLARE_OPERATORS_FOR_FLAGS(MotionTypes)


    enum PanicMode {
        PM_None = 0,
        PM_BusinessEvents = 1,
        PM_User = 2
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(PanicMode)

    enum ConnectionRole {
        CR_Default,         /// In client this flag is sufficient to receive both archive and live video
        CR_LiveVideo,
        CR_SecondaryLiveVideo,
        CR_Archive
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(Qn::ConnectionRole)

    //TODO: #GDM split to server-only and client-only flags as they are always local
    enum ResourceFlag {
        network                     = 0x1,          /**< Has ip and mac. */
        url                         = 0x2,          /**< Has url, e.g. file name. */
        streamprovider              = 0x4,
        media                       = 0x8,

        playback                    = 0x10,         /**< Something playable (not real time and not a single shot). */
        video                       = 0x20,
        audio                       = 0x40,
        live                        = 0x80,

        still_image                 = 0x100,        /**< Still image device. */
        local                       = 0x200,        /**< Local client resource. */
        server                      = 0x400,        /**< Server resource. */
        remote                      = 0x800,        /**< Remote (on-server) resource. */

        layout                      = 0x1000,       /**< Layout resource. */
        user                        = 0x2000,       /**< User resource. */
        utc                         = 0x4000,       /**< Resource uses UTC-based timing. */
        periods                     = 0x8000,       /**< Resource has recorded periods. */

        motion                      = 0x10000,      /**< Resource has motion */
        sync                        = 0x20000,      /**< Resource can be used in sync playback mode. */
        foreigner                   = 0x40000,      /**< Resource belongs to other entity. E.g., camera on another server */
        no_last_gop                 = 0x80000,      /**< Do not use last GOP for this when stream is opened */

        deprecated                  = 0x100000,     /**< Resource absent in Server but still used in memory for some reason */
        videowall                   = 0x200000,     /**< Videowall resource */
        desktop_camera              = 0x400000,     /**< Desktop Camera resource */
        parent_change               = 0x800000,     /**< Camera discovery internal purpose */

        depend_on_parent_status     = 0x1000000,    /**< Resource status depend on parent resource status */
        search_upd_only             = 0x2000000,    /**< Disable to insert new resource during discovery process, allow update only */
        io_module                   = 0x4000000,    /**< It's I/O module camera (camera subtype) */
        read_only                   = 0x8000000,    /**< Resource is read-only by design, e.g. server in safe mode. */

        storage_fastscan            = 0x10000000,   /**< Fast scan for storage in progress */

        local_media = local | media,
        local_layout = local | layout,

        local_server = local | server,
        remote_server = remote | server,
        safemode_server = read_only | server,

        live_cam = utc | sync | live | media | video | streamprovider, // don't set w/o `local` or `remote` flag
        local_live_cam = live_cam | local | network,
        server_live_cam = live_cam | remote,// | network,
        server_archive = remote | media | video | audio | streamprovider,
        local_video = url | local | media | video | audio | streamprovider,     /**< Local media file. */
        local_image = url | local | media | still_image | streamprovider,    /**< Local still image file. */

        web_page = url | remote,   /**< Web-page resource */
    };
    Q_DECLARE_FLAGS(ResourceFlags, ResourceFlag)
    Q_DECLARE_OPERATORS_FOR_FLAGS(ResourceFlags)

    enum ResourceStatus
    {
        Offline,
        Unauthorized,
        Online,
        Recording,
        NotDefined,
        /*! Applies only to a server resource. A server is incompatible only when it has system name different
         * from the current or it has incompatible protocol version.
         * \note Incompatible server is not the same as fake server which is create in the client by
         * QnIncompatibleServerWatcher. Fake servers can also have Unauthorized status.
         * So if you want to check if the server is fake use QnMediaServerResource::isFakeServer().
         */
        Incompatible,

        AnyStatus
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(ResourceStatus)

    /** Level of detail for displaying resource info. */
    enum ResourceInfoLevel
    {
        RI_Invalid,
        RI_NameOnly,       /**< Only resource name */
        RI_WithUrl,        /**< Resource name and url (if exist) */
        RI_FullInfo,       /**< All info */
		RI_UrlOnly         /** Only url */
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(ResourceInfoLevel)

    enum BitratePerGopType {
        BPG_None,
        BPG_Predefined,
        BPG_User
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(BitratePerGopType)

    // TODO: #Elric #EC2 talk to Roma, write comments
    enum ServerFlag {
        SF_None             = 0x000,
        SF_Edge             = 0x001,
        SF_RemoteEC         = 0x002,
        SF_HasPublicIP      = 0x004,
        SF_IfListCtrl       = 0x008,
        SF_timeCtrl         = 0x010,
        //SF_AutoSystemName   = 0x020,        /**< System name is default, so it will be displayed as "Unassigned System' in NxTool. */
        SF_ArmServer        = 0x040,
        SF_Has_HDD          = 0x080,
        SF_NewSystem        = 0x100,        /**< System is just installed, it has default admin password and is not linked to the cloud. */
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(ServerFlag)

    Q_DECLARE_FLAGS(ServerFlags, ServerFlag)
    Q_DECLARE_OPERATORS_FOR_FLAGS(ServerFlags)


    enum TimeFlag
    {
        TF_none = 0x0,
        TF_peerIsNotEdgeServer = 0x0001,
        TF_peerHasMonotonicClock = 0x0002,
        TF_peerTimeSetByUser = 0x0004,
        TF_peerTimeSynchronizedWithInternetServer = 0x0008,
        TF_peerIsServer = 0x1000
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(TimeFlag)

    Q_DECLARE_FLAGS(TimeFlags, TimeFlag)
    Q_DECLARE_OPERATORS_FOR_FLAGS(TimeFlags)


    enum IOPortType {
        PT_Unknown  = 0x0,
        PT_Disabled = 0x1,
        PT_Input    = 0x2,
        PT_Output   = 0x4
    };
    Q_DECLARE_FLAGS(IOPortTypes, IOPortType)
    Q_DECLARE_OPERATORS_FOR_FLAGS(IOPortTypes)
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(IOPortType)

    enum AuditRecordType
    {
        AR_NotDefined        = 0x0000,
        AR_UnauthorizedLogin = 0x0001,
        AR_Login             = 0x0002,
        AR_UserUpdate        = 0x0004,
        AR_ViewLive          = 0x0008,
        AR_ViewArchive       = 0x0010,
        AR_ExportVideo       = 0x0020,
        AR_CameraUpdate      = 0x0040,
        AR_SystemNameChanged = 0x0080,
        AR_SystemmMerge      = 0x0100,
        AR_SettingsChange    = 0x0200,
        AR_ServerUpdate      = 0x0400,
        AR_BEventUpdate      = 0x0800,
        AR_EmailSettings     = 0x1000,
        AR_CameraRemove      = 0x2000,
        AR_ServerRemove      = 0x4000,
        AR_BEventRemove      = 0x8000,
        AR_UserRemove        = 0x10000,
        AR_BEventReset       = 0x20000,
        AR_DatabaseRestore   = 0x40000,
        AR_CameraInsert      = 0x80000
    };

    Q_DECLARE_FLAGS(AuditRecordTypes, AuditRecordType)
    Q_DECLARE_OPERATORS_FOR_FLAGS(AuditRecordTypes)
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(AuditRecordType)

    enum IODefaultState {
        IO_OpenCircuit,
        IO_GroundedCircuit
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(IODefaultState)

    enum TimePeriodType {
        NullTimePeriod      = 0x1,  /**< No period. */
        EmptyTimePeriod     = 0x2,  /**< Period of zero length. */
        NormalTimePeriod    = 0x4,  /**< Normal period with non-zero length. */
    };

    Q_DECLARE_FLAGS(TimePeriodTypes, TimePeriodType)
    Q_DECLARE_OPERATORS_FOR_FLAGS(TimePeriodTypes)


    enum TimePeriodContent {
        RecordingContent,
        MotionContent,

        TimePeriodContentCount
    };

    enum SystemComponent {
        ServerComponent,
        ClientComponent,
        AnyComponent
    };


    /**
     * Generic enumeration holding different data roles used in Qn classes.
     */
    enum ItemDataRole {
        FirstItemDataRole   = Qt::UserRole,

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
        LayoutCellSpacingRole,                      /**< Role for layout's cell spacing. Value of type QSizeF. */
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
        TimePeriodRole,
        TimePeriodsRole,
        MergedTimePeriodsRole,
        FileNameRole,                               /**< Role for target filename. Used in TakeScreenshotAction. */
        TitleRole,                                  /**< Role for dialog title. Used in MessageBoxAction. */
        TextRole,                                   /**< Role for dialog text. Used in MessageBoxAction. */
        UrlRole,                                    /**< Role for target url. Used in BrowseUrlAction and QnActions::ConnectAction. */
        ConnectionAliasRole,                        /**< Role for alias of connection. Used in QnActions::ConnectAction, QnLoginDialog. */
        AutoLoginRole,                              /**< Role for flag that shows if client should connect with last credentials
                                                        (or to the last system) automatically next time */
        StorePasswordRole,                          /**< Role for flag that shows if password of successful connection should be stored.
                                                         Used in QnActions::ConnectAction. */
        ForceRole,                                  /**< Role for 'forced' flag. Used in QnActions::DisconnectAction */
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
        EventResourcesRole,                         /**< Role for business event resources list. Value of type QnResourceList. */
        ActionTypeRole,                             /**< Role for business action type. Value of type QnBusiness::ActionType. */
        ActionResourcesRole,                        /**< Role for business action resources list. Value of type QnResourceList. */

        SoftwareVersionRole,                        /**< Role for software version. Value of type QnSoftwareVersion. */

        StorageUrlRole,                             /**< Role for storing real storage Url in storage_url_dialog. */

        IOPortDataRole,                             /**< Return QnIOPortData object. Used in IOPortDataModel */

        RecordingStatsDataRole,                     /**< Return QnCamRecordingStatsData object. Used in QnRecordingStatsModel */
        RecordingStatChartDataRole,                 /**< Return qreal for chart. Real value. Used in QnRecordingStatsModel */
        RecordingStatChartColorDataRole,            /**< Return QnRecordingStatsColors. Used in QnRecordingStatsModel */

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
        RoleCount
    };

    // TODO: #Elric #EC2 rename
    enum StreamQuality {
        QualityLowest = 0,
        QualityLow = 1,
        QualityNormal = 2,
        QualityHigh = 3,
        QualityHighest = 4,
        QualityPreSet = 5,
        QualityNotDefined = 6,

        StreamQualityCount
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(StreamQuality)


    // TODO: #Elric #EC2 rename
    enum SecondStreamQuality {
        SSQualityLow = 0,
        SSQualityMedium = 1,
        SSQualityHigh = 2,
        SSQualityNotDefined = 3,
        SSQualityDontUse = 4
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(SecondStreamQuality)

    enum StatisticsDeviceType {
        StatisticsCPU = 0,                /**< CPU load in percents. */
        StatisticsRAM = 1,                /**< RAM load in percents. */
        StatisticsHDD = 2,                /**< HDD load in percents. */
        StatisticsNETWORK = 3             /**< Network load in percent. */
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(StatisticsDeviceType)


    enum CameraStatusFlag {
        CSF_NoFlags = 0x0,
        CSF_HasIssuesFlag = 0x1
    };
    Q_DECLARE_FLAGS(CameraStatusFlags, CameraStatusFlag)
    Q_DECLARE_OPERATORS_FOR_FLAGS(CameraStatusFlags)
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(CameraStatusFlag)

    // TODO: #Elric #EC2 rename
    enum RecordingType {
        /** Record always. */
        RT_Always = 0,

        /** Record only when motion was detected. */
        RT_MotionOnly = 1,

        /** Don't record. */
        RT_Never = 2,

        /** Record LQ stream always and HQ stream only on motion. */
        RT_MotionAndLowQuality = 3,

        RT_Count
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(RecordingType)

    enum PeerType {
        PT_NotDefined = -1,
        PT_Server = 0,
        PT_DesktopClient = 1,
        PT_VideowallClient = 2,
        PT_MobileClient = 3,
        PT_Count
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(PeerType)

    enum PropertyDataType {
        PDT_None        = 0,
        PDT_Value       = 1,
        PDT_OnOff       = 2,
        PDT_Boolen      = 3,
        PDT_MinMaxStep  = 4,
        PDT_Enumeration = 5,
        PDT_Button      = 6
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(PropertyDataType)


    enum SerializationFormat {
        JsonFormat          = 0,
        UbjsonFormat        = 1,
        BnsFormat           = 2,
        CsvFormat           = 3,
        XmlFormat           = 4,
        CompressedPeriodsFormat = 5, // used for chunks data only
        UrlQueryFormat      = 6,     //will be added in future for parsing url query (e.g., name1=val1&name2=val2)

        UnsupportedFormat   = -1
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(SerializationFormat)

    const char* serializationFormatToHttpContentType(SerializationFormat format);
    SerializationFormat serializationFormatFromHttpContentType(const QByteArray& httpContentType);

    enum TTHeaderFlag
    {
        TT_None          = 0x0,
        TT_ProxyToClient = 0x1
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(TTHeaderFlag)
    Q_DECLARE_FLAGS(TTHeaderFlags, TTHeaderFlag)
    Q_DECLARE_OPERATORS_FOR_FLAGS(TTHeaderFlags)

    enum LicenseType
    {
        LC_Trial,
        LC_Analog,
        LC_Professional,
        LC_Edge,
        LC_VMAX,
        LC_AnalogEncoder,
        LC_VideoWall,

        /**
         * I/O Modules license.
         * Needs to be activated to enable I/O module features. One license channel per one module.
         */
        LC_IO,

        /**
         * Like a professional license.
         * Could not be activated on ARM devices.
         * Only one license key per system (not server). If systems are merged and each of them had some start licenses originally,
         * new merged system will only take one start license( the one with bigger channels). Other start licenses will become invalid.
         */
        LC_Start,

        /**
         * Invalid license. Required when the correct license type is not known in current version.
         */
        LC_Invalid,

        LC_Count
    };

    // All columns are sorted by database initially, except camera name and tags.
    enum BookmarkSortField
    {
        BookmarkName
        , BookmarkStartTime
        , BookmarkDuration
        , BookmarkTags          // Sorted manually!
        , BookmarkCameraName    // Sorted manually!
    };

    /**
    * Authentication error code
    */
    enum AuthResult
    {
        Auth_OK,            // OK
        Auth_WrongLogin,    // invalid login
        Auth_WrongInternalLogin, // invalid login used for internal auth scheme
        Auth_WrongDigest,   // invalid or empty digest
        Auth_WrongPassword, // invalid password
        Auth_Forbidden,     // no auth mehod found or custom auth scheme without login/password is failed
        Auth_PasswordExpired, // Password is expired
        Auth_ConnectError   // can't connect to the external system to authenticate
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(AuthResult)

    enum FailoverPriority {
        FP_Never,
        FP_Low,
        FP_Medium,
        FP_High,

        FP_Count
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(FailoverPriority)
    static_assert(FP_Medium == 2, "Value is hardcoded in SQL migration script.");

    // TODO: #MSAPI move to api/model or even to common_globals,
    // add lexical serialization (see QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS)
    //
    // Check serialization/deserialization in QnMediaServerConnection::doRebuildArchiveAsync
    // and in handler.
    //
    // And name them sanely =)
    enum RebuildAction
    {
        RebuildAction_ShowProgress,
        RebuildAction_Start,
        RebuildAction_Cancel
    };

    enum BackupAction
    {
        BackupAction_ShowProgress,
        BackupAction_Start,
        BackupAction_Cancel
    };


    /**
     * backup settings
     */
    enum BackupType
    {
        Backup_Manual,
        Backup_RealTime,
        Backup_Schedule
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(BackupType)

    enum CameraBackupQuality
    {
        CameraBackup_Disabled       = 0,
        CameraBackup_HighQuality    = 1,
        CameraBackup_LowQuality     = 2,
        CameraBackup_Both           = CameraBackup_HighQuality | CameraBackup_LowQuality,
        CameraBackup_Default        = 4 // backup type didn't configured so far. Default value will be used
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(CameraBackupQuality)
    Q_DECLARE_FLAGS(CameraBackupQualities, CameraBackupQuality)
    Q_DECLARE_OPERATORS_FOR_FLAGS(CameraBackupQualities)

    /**
     * Flags describing the actions permitted for the user to do with the
     * selected resource. Calculated in runtime.
     */
    enum Permission
    {
        /* Generic permissions. */
        NoPermissions                   = 0x0000,   /**< No access */

        ReadPermission                  = 0x0001,   /**< Generic read access. Having this access right doesn't necessary mean that all information is readable. */
        WritePermission                 = 0x0002,   /**< Generic write access. Having this access right doesn't necessary mean that all information is writable. */
        SavePermission                  = 0x0004,   /**< Generic save access. Entity can be saved to the server. */
        RemovePermission                = 0x0008,   /**< Generic delete permission. */
        ReadWriteSavePermission = ReadPermission | WritePermission | SavePermission,
        WriteNamePermission             = 0x0010,   /**< Permission to edit resource's name. */

        /* Layout-specific permissions. */
        AddRemoveItemsPermission        = 0x0020,   /**< Permission to add or remove items from a layout. */
        EditLayoutSettingsPermission    = 0x0040,   /**< Permission to setup layout background or set locked flag. */
        ModifyLayoutPermission          = Qn::ReadPermission | Qn::WritePermission | Qn::AddRemoveItemsPermission, /**< Permission to modify without saving. */
        FullLayoutPermissions           = ReadWriteSavePermission | WriteNamePermission | RemovePermission | AddRemoveItemsPermission | EditLayoutSettingsPermission,

        /* User-specific permissions. */
        WritePasswordPermission         = 0x0200,   /**< Permission to edit associated password. */
        WriteAccessRightsPermission     = 0x0400,   /**< Permission to edit access rights. */
        ReadEmailPermission             = ReadPermission,
        WriteEmailPermission            = WritePasswordPermission,
        WriteFullNamePermission         = WritePasswordPermission,
        FullUserPermissions             = ReadWriteSavePermission | WriteNamePermission | RemovePermission |
                                            WritePasswordPermission | WriteAccessRightsPermission,

        /* Media-specific permissions. */
        ExportPermission                = 0x800,   /**< Permission to export video parts. */

        /* Camera-specific permissions. */
        WritePtzPermission              = 0x1000,   /**< Permission to use camera's PTZ controls. */

        AllPermissions = 0xFFFFFFFF
    };

    Q_DECLARE_FLAGS(Permissions, Permission)
    Q_DECLARE_OPERATORS_FOR_FLAGS(Permissions)
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(Permission)



    /**
     * Flags describing global user capabilities, independently of resources. Stored in the database.
     * QFlags uses int internally, so we are limited to 32 bits.
     */
    enum GlobalPermission
    {
        /* Generic permissions. */
        NoGlobalPermissions                     = 0x00000000,   /**< Only live video access. */

        /* Admin permissions. */
        GlobalAdminPermission                   = 0x00000001,   /**< Admin, can edit other non-admins. */

        /* Manager permissions. */
        GlobalEditCamerasPermission             = 0x00000002,   /**< Can edit camera settings. */
        GlobalControlVideoWallPermission        = 0x00000004,   /**< Can control videowalls. */
        GlobalViewLogsPermission                = 0x00000008,   /**< Can access event log and audit trail. */

        /* Viewer permissions. */
        GlobalViewArchivePermission             = 0x00000100,   /**< Can view archives of available cameras. */
        GlobalExportPermission                  = 0x00000200,   /**< Can export archives of available cameras. */
        GlobalViewBookmarksPermission           = 0x00000400,   /**< Can view bookmarks of available cameras. */
        GlobalManageBookmarksPermission         = 0x00000800,   /**< Can modify bookmarks of available cameras. */

        /* Input permissions. */
        GlobalUserInputPermission               = 0x00010000,   /**< Can change camera's PTZ state, use 2-way audio, I/O buttons. */

        /* Resources access permissions */
        GlobalAccessAllCamerasPermission        = 0x01000000,   /**< Has access to all cameras. */


        /* Shortcuts. */

        /* Live viewer has access to all cameras and global layouts by default. */
        GlobalLiveViewerPermissionSet       = GlobalAccessAllCamerasPermission,

        /* Viewer can additionally view archive and bookmarks and export video. */
        GlobalViewerPermissionSet           = GlobalLiveViewerPermissionSet | GlobalViewArchivePermission | GlobalExportPermission | GlobalViewBookmarksPermission,

        /* Advanced viewer can manage bookmarks and use various input methods. */
        GlobalAdvancedViewerPermissionSet   = GlobalViewerPermissionSet | GlobalManageBookmarksPermission | GlobalUserInputPermission | GlobalViewLogsPermission,

        /* Admin can do everything. */
        GlobalAdminPermissionsSet           = GlobalAdminPermission | GlobalAdvancedViewerPermissionSet | GlobalControlVideoWallPermission | GlobalEditCamerasPermission,

        /* PTZ here is intended - for SpaceX, see VMS-2208 */
        GlobalVideoWallModePermissionSet    = GlobalLiveViewerPermissionSet | GlobalViewArchivePermission | GlobalUserInputPermission,

        /* Actions in ActiveX plugin mode are limited. */
        GlobalActiveXModePermissionSet      = GlobalViewerPermissionSet | GlobalUserInputPermission,
    };

    Q_DECLARE_FLAGS(GlobalPermissions, GlobalPermission)
    Q_DECLARE_OPERATORS_FOR_FLAGS(GlobalPermissions)
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(GlobalPermission)



    /**
     * Invalid value for a timezone UTC offset.
     */
    static const qint64 InvalidUtcOffset = std::numeric_limits<qint64>::max();
#define InvalidUtcOffset InvalidUtcOffset

    /**
     * Helper function that can be used to 'place' macros into Qn namespace.
     */
    template<class T>
    const T &_id(const T &value) { return value; }

} // namespace Qn


// TODO: #Elric #enum

enum {MD_WIDTH = 44, MD_HEIGHT = 32};


/** Time value for 'now'. */
#define DATETIME_NOW        std::numeric_limits<qint64>::max()

// TODO: #rvasilenko Change to other constant - 0 is 1/1/1970
// Note: -1 is used for invalid time
// Now it is returning when no archive data and archive is played backward
enum { kNoTimeValue = 0 };

/** Time value for 'unknown' / 'invalid'. Same as AV_NOPTS_VALUE. Checked in ffmpeg.cpp. */
#define DATETIME_INVALID    std::numeric_limits<qint64>::min()


/**
 * \def lit
 * Helper macro to mark strings that are not to be translated.
 */
#define QN_USE_QT_STRING_LITERALS
#ifdef QN_USE_QT_STRING_LITERALS
namespace QnLitDetail { template<int N> void check_string_literal(const char (&)[N]) {} }
#   define lit(s) (QnLitDetail::check_string_literal(s), QStringLiteral(s))
#else
#   define lit(s) QLatin1String(s)
#endif

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (Qn::TimePeriodContent)(Qn::Corner),
    (metatype)
)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (Qn::PtzObjectType)(Qn::PtzCommand)(Qn::PtzTrait)(Qn::PtzTraits)(Qn::PtzCoordinateSpace)(Qn::MotionType)
        (Qn::StreamQuality)(Qn::SecondStreamQuality)(Qn::StatisticsDeviceType)
        (Qn::ServerFlag)(Qn::BackupType)(Qn::CameraBackupQuality)
        (Qn::PanicMode)(Qn::RecordingType)
        (Qn::ConnectionRole)(Qn::ResourceStatus)(Qn::BitratePerGopType)
        (Qn::SerializationFormat)(Qn::PropertyDataType)(Qn::PeerType)(Qn::RebuildState)(Qn::BackupState)
        (Qn::BookmarkSortField)(Qt::SortOrder)
        (Qn::RebuildAction)(Qn::BackupAction)
        (Qn::TTHeaderFlag)(Qn::IOPortType)(Qn::IODefaultState)(Qn::AuditRecordType)(Qn::AuthResult)
        (Qn::FailoverPriority)
        ,
    (metatype)(lexical)
)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (Qn::PtzCapabilities)(Qn::ServerFlags)(Qn::CameraBackupQualities)(Qn::TimeFlags)(Qn::CameraStatusFlags)
    (Qn::Permission)(Qn::GlobalPermission)(Qn::Permissions)(Qn::GlobalPermissions)(Qn::IOPortTypes)
    ,
    (metatype)(numeric)(lexical)
)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (Qn::PtzDataFields)(Qn::TTHeaderFlags)(Qn::ResourceInfoLevel),
    (metatype)(numeric)
)

