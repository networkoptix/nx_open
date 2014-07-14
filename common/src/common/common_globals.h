#ifndef QN_COMMON_GLOBALS_H
#define QN_COMMON_GLOBALS_H

#include <QtCore/QtGlobal>
#include <QtCore/QMetaType>
#include <QtCore/QString>

#include <utils/math/defines.h> /* For INT64_MAX. */
#include <utils/common/unused.h>
#include <utils/common/model_functions_fwd.h>

/**
 * Same as <tt>Q_GADGET</tt>, but doesn't trigger MOC, and can be used in namespaces.
 * The only sane use case is for generating metainformation for enums in
 * namespaces (e.g. for use with <tt>QnEnumNameMapper</tt>).
 */
#define Q_NAMESPACE                                                             \
    extern const QMetaObject staticMetaObject;                                  \
    extern const QMetaObject &getStaticMetaObject();                            \


#ifdef Q_MOC_RUN
class Qn
#else
namespace Qn
#endif
{
#ifdef Q_MOC_RUN
    Q_GADGET
    Q_ENUMS(Border Corner ExtrapolationMode CameraCapability PtzObjectType PtzCommand PtzDataField PtzCoordinateSpace CameraDataType
            PtzCapability StreamFpsSharingMethod MotionType TimePeriodType TimePeriodContent SystemComponent ItemDataRole 
            StreamQuality SecondStreamQuality PanicMode RecordingType PropertyDataType SerializationFormat PeerType)
    Q_FLAGS(Borders Corners CameraCapabilities PtzDataFields PtzCapabilities PtzTraits MotionTypes TimePeriodTypes ServerFlags CameraStatusFlags)
public:
#else
    Q_NAMESPACE
#endif

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
        ShareIpCapability                   = 0x020 
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

        builtinPresetControl                = 0x02000000,

        /* Shortcuts */
        ContinuousPanTiltCapabilities       = ContinuousPanCapability | ContinuousTiltCapability,
        ContinuousPtzCapabilities           = ContinuousPanCapability | ContinuousTiltCapability | ContinuousZoomCapability,
        AbsolutePtzCapabilities             = AbsolutePanCapability | AbsoluteTiltCapability | AbsoluteZoomCapability,
        nativePresetsPtzCapability          = PresetsPtzCapability | builtinPresetControl,
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(PtzCapability)

    Q_DECLARE_FLAGS(PtzCapabilities, PtzCapability)
    Q_DECLARE_OPERATORS_FOR_FLAGS(PtzCapabilities)



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


    // TODO: #Elric #EC2 talk to Roma, write comments
    enum ServerFlag { 
        SF_None         = 0x0, 
        SF_Edge         = 0x1,
        SF_RemoteEC     = 0x2,
        SF_HasPublicIP  = 0x4
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(ServerFlag)

    Q_DECLARE_FLAGS(ServerFlags, ServerFlag)
    Q_DECLARE_OPERATORS_FOR_FLAGS(ServerFlags);


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
        BookmarksContent,
        TimePeriodContentCount
    };

    enum CameraDataType {
        RecordedTimePeriod,
        MotionTimePeriod,
        BookmarkTimePeriod,
        BookmarkData,

        CameraDataTypeCount
    };

    enum SystemComponent {
        EnterpriseControllerComponent,
        MediaServerComponent,
        ClientComponent,
        MediaProxyComponent,

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
        ResourceFlagsRole,                          /**< Role for resource flags. Value of type int (QnResource::Flags). */
        ResourceSearchStringRole,                   /**< Role for resource search string. Value of type QString. */
        ResourceStatusRole,                         /**< Role for resource status. Value of type int (QnResource::Status). */
        ResourceUidRole,                            /**< Role for resource unique id. Value of type QString. */

        VideoWallGuidRole,                          /**< Role for videowall resource unique id. Value of type QUuid. */
        VideoWallItemGuidRole,                      /**< Role for videowall item unique id. Value of type QUuid. */
        VideoWallPcGuidRole,                        /**< Role for videowall pc unique id. Value of type QUuid. */
        VideoWallPcScreenIndicesRole,               /**< Role for videowall pc screen indices. Value of type QSet<int>. */

        /* Layout-based. */
        LayoutCellSpacingRole,                      /**< Role for layout's cell spacing. Value of type QSizeF. */
        LayoutCellAspectRatioRole,                  /**< Role for layout's cell aspect ratio. Value of type qreal. */
        LayoutBoundingRectRole,                     /**< Role for layout's bounding rect. Value of type QRect. */
        LayoutSyncStateRole,                        /**< Role for layout's stream synchronization state. Value of type QnStreamSynchronizationState. */
        LayoutSearchStateRole,                      /**< */
        LayoutTimeLabelsRole,                       /**< Role for layout's time label diplay. Value of type bool. */ 
        LayoutPermissionsRole,                      /**< Role for overriding layout's permissions. Value of type int (Qn::Permissions). */ 
        LayoutSelectionRole,                        /**< Role for layout's selected items. Value of type QVector<QUuid>. */

        /* Item-based. */
        ItemUuidRole,                               /**< Role for item's UUID. Value of type QUuid. */
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

        ItemTimeRole,                               /**< Role for item's playback position, in milliseconds. Value of type qint64. Default value is -1. */
        ItemThumbnailTimestampRole,                 /**< Role for item's loaded thumbnail timestamp, in milliseconds. Used in thumbnails search. Value of type qint64. */
        ItemPausedRole,                             /**< Role for item's paused state. Value of type bool. */
        ItemSpeedRole,                              /**< Role for item's playback speed. Value of type qreal. */
        ItemSliderWindowRole,                       /**< Role for slider window that is displayed when the item is active. Value of type QnTimePeriod. */
        ItemSliderSelectionRole,                    /**< Role for slider selection that is displayed when the items is active. Value of type QnTimePeriod. */
        ItemCheckedButtonsRole,                     /**< Role for buttons that are checked in item's titlebar. Value of type int (QnResourceWidget::Buttons). */
        ItemDisabledButtonsRole,                    /**< Role for buttons that are not to be displayed in item's titlebar. Value of type int (QnResourceWidget::Buttons). */
        ItemHealthMonitoringButtonsRole,            /**< Role for buttons that are checked on each line of Health Monitoring widget. Value of type QnServerResourceWidget::HealthMonitoringButtons. */
        ItemVideowallReviewButtonsRole,             /**< Role for buttons that are checked on each sub-item of the videowall screen widget. Value of type QnVideowallScreenWidget::ReviewButtons. */

        /* Ptz-based. */
        PtzPresetRole,                              /**< Role for PTZ preset. Value of type QnPtzPreset. */
        PtzTourRole,                                /**< Role for PTZ tour. Value of type QnPtzTour. */
        PtzObjectIdRole,                            /**< Role for PTZ tour/preset id. Value of type QString. */
        PtzObjectNameRole,                          /**< Role for PTZ tour/preset name. Value of type QString. */
        PtzTourSpotRole,                            /**< Role for PTZ tour spot. Value of type QnPtzTourSpot. */

        /* Context-based. */
        CurrentLayoutResourceRole,
        CurrentUserResourceRole,
        CurrentLayoutMediaItemsRole,
        CurrentMediaServerResourcesRole,

        /* Arguments. */
        ActionIdRole,
        SerializedDataRole,
        ConnectionInfoRole,
        FocusElementRole,
        TimePeriodRole,
        TimePeriodsRole,
        MergedTimePeriodsRole,
        AutoConnectRole,
        FileNameRole,                               /**< Role for target filename. Used in TakeScreenshotAction. */
        TitleRole,                                  /**< Role for dialog title. Used in MessageBoxAction. */
        TextRole,                                   /**< Role for dialog text. Used in MessageBoxAction. */
        UrlRole,                                    /**< Role for target url. Used in BrowseUrlAction. */
        ForceRole,                                  /**< Role for 'forced' flag. Used in DisconnectAction */
        CameraBookmarkRole,                         /**< Role for the selected camera bookmark (if any). Used in Edit/RemoveCameraBookmarkAction */
        UuidRole,                                   /**< Role for target uuid. Used in LoadVideowallMatrixAction. */
        KeyboardModifiersRole,                      /**< Role for keyboard modifiers. Used in some Drop actions. */

        /* Others. */
        HelpTopicIdRole,                            /**< Role for item's help topic. Value of type int. */
        
        TranslationRole,                            /**< Role for translations. Value of type QnTranslation. */

        ItemMouseCursorRole,                        /**< Role for item's mouse cursor. */
        DisplayHtmlRole,                            /**< Same as Display role, but use HTML format. */

        ModifiedRole,                               /**< Role for modified state. Value of type bool. */
        DisabledRole,                               /**< Role for disabled state. Value of type bool. */
        ValidRole,                                  /**< Role for valid state. Value of type bool. */
        ActionIsInstantRole,                        /**< Role for instant state for business rule actions. Value of type bool. */
        ShortTextRole,                              /**< Role for short text. Value of type QString. */

        EventTypeRole,                              /**< Role for business event type. Value of type QnBusiness::EventType. */
        EventResourcesRole,                         /**< Role for business event resources list. Value of type QnResourceList. */
        ActionTypeRole,                             /**< Role for business action type. Value of type QnBusiness::ActionType. */
        ActionResourcesRole,                        /**< Role for business action resources list. Value of type QnResourceList. */

        SoftwareVersionRole,                        /**< Role for software version. Value of type QnSoftwareVersion. */

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
        JsonFormat      = 0,
        UbjsonFormat    = 1,
        BnsFormat       = 2,
        CsvFormat       = 3,
        XmlFormat       = 4
    };
    QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(SerializationFormat)


    /**
     * Invalid value for a timezone UTC offset.
     */
    static const qint64 InvalidUtcOffset = INT64_MAX;
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
#define DATETIME_NOW        INT64_MAX 

/** Time value for 'unknown' / 'invalid'. Same as AV_NOPTS_VALUE. Checked in ffmpeg.cpp. */
#define DATETIME_INVALID    INT64_MIN



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
    (Qn::TimePeriodContent)(Qn::Corner)(Qn::CameraDataType), 
    (metatype)
)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (Qn::PtzObjectType)(Qn::PtzCommand)(Qn::PtzTrait)(Qn::PtzCoordinateSpace)(Qn::MotionType)
        (Qn::StreamQuality)(Qn::SecondStreamQuality)(Qn::ServerFlag)(Qn::PanicMode)(Qn::RecordingType)
        (Qn::SerializationFormat)(Qn::PropertyDataType)(Qn::PeerType), 
    (metatype)(lexical)
)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (Qn::ServerFlags)(Qn::PtzDataFields)(Qn::PtzCapabilities)(Qn::CameraStatusFlags),
    (metatype)(numeric)
)

#endif // QN_COMMON_GLOBALS_H
