#ifndef QN_COMMON_GLOBALS_H
#define QN_COMMON_GLOBALS_H

#include <QtCore/QtGlobal>
#include <QtCore/QMetaType>
#include <QtCore/QString>

#include <utils/math/limits.h> /* For INT64_MAX. */

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
    Q_ENUMS(ExtrapolationMode MotionType StreamFpsSharingMethod)
    Q_FLAGS(CameraCapabilities PtzCapabilities)
public:
#else
    Q_NAMESPACE
#endif

        // TODO: #5.0 use Qt::Edge
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
        shareIpCapability                   = 0x020
    };
    Q_DECLARE_FLAGS(CameraCapabilities, CameraCapability);
    Q_DECLARE_OPERATORS_FOR_FLAGS(CameraCapabilities);


    enum PtzCapability {
        NoPtzCapabilities                   = 0x000,
        AbsolutePtzCapability               = 0x020,
        ContinuousPanTiltCapability         = 0x040,
        ContinuousZoomCapability            = 0x080,
        OctagonalPtzCapability              = 0x100, // TODO: #Elric deprecate this shit. Not really a capability.

        /* Shortcuts */
        AllPtzCapabilities                  = AbsolutePtzCapability | ContinuousPanTiltCapability | ContinuousZoomCapability | OctagonalPtzCapability,

        /* Deprecated capabilities. */
        DeprecatedContinuousPtzCapability   = 0x001,
        DeprecatedZoomCapability            = 0x002,
    };
    Q_DECLARE_FLAGS(PtzCapabilities, PtzCapability);
    Q_DECLARE_OPERATORS_FOR_FLAGS(PtzCapabilities);

    /**
     * \param capabilities              Camera capability flags containing some deprecated values.
     * \returns                         Camera capability flags with deprecated values replaced with new ones.
     */
    inline Qn::PtzCapabilities undeprecatePtzCapabilities(Qn::PtzCapabilities capabilities) {
        Qn::PtzCapabilities result = capabilities;

        if(result & Qn::DeprecatedContinuousPtzCapability) {
            result &= ~Qn::DeprecatedContinuousPtzCapability;
            result |= Qn::ContinuousPanTiltCapability | Qn::ContinuousZoomCapability;
        }

        if(result & Qn::DeprecatedZoomCapability) {
            result &= ~Qn::DeprecatedZoomCapability;
            result |= Qn::ContinuousZoomCapability;
        }

        return result;
    }


    enum StreamFpsSharingMethod {
        shareFps, // if second stream is running whatever fps it has => first stream can get maximumFps - secondstreamFps
        sharePixels, //if second stream is running whatever megapixel it has => first stream can get maxMegapixels - secondstreamPixels
        noSharing // second stream does not subtract first stream fps 
    };

    enum MotionType {
        MT_Default = 0, 
        MT_HardwareGrid = 1, 
        MT_SoftwareGrid = 2, 
        MT_MotionWindow = 4, 
        MT_NoMotion = 8
    };
    Q_DECLARE_FLAGS(MotionTypes, MotionType);


    enum TimePeriodType {
        NullTimePeriod      = 0x1,  /**< No period. */
        EmptyTimePeriod     = 0x2,  /**< Period of zero length. */
        NormalTimePeriod    = 0x4,  /**< Normal period with non-zero length. */
    };
    Q_DECLARE_FLAGS(TimePeriodTypes, TimePeriodType);
    Q_DECLARE_OPERATORS_FOR_FLAGS(TimePeriodTypes);


    enum TimePeriodContent {
        RecordingContent,
        MotionContent,
        TimePeriodContentCount
    };


    enum ToggleState {
        OffState,
        OnState,
        UndefinedState /**< Also used in event rule to associate non-toggle action with event with any toggle state. */
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
        NodeTypeRole        = FirstItemDataRole,    /**< Role for node type, see <tt>Qn::NodeType</tt>. */

        /* Resource-based. */
        ResourceRole,                               /**< Role for QnResourcePtr. */
        UserResourceRole,                           /**< Role for QnUserResourcePtr. */
        LayoutResourceRole,                         /**< Role for QnLayoutResourcePtr. */
        MediaServerResourceRole,                    /**< Role for QnMediaServerResourcePtr. */
        ResourceNameRole,                           /**< Role for resource name. Value of type QString. */
        ResourceFlagsRole,                          /**< Role for resource flags. Value of type int (QnResource::Flags). */
        ResourceSearchStringRole,                   /**< Role for resource search string. Value of type QString. */
        ResourceStatusRole,                         /**< Role for resource status. Value of type int (QnResource::Status). */
        ResourceUidRole,                            /**< Role for resource unique id. Value of type QString. */

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
        ItemImageDewarpingRole,                     /**< Role for item's image dewarping params. Value of type DewarpingParams. */
        ItemFlagsRole,                              /**< Role for item's flags. Value of type int (Qn::ItemFlags). */
        ItemRotationRole,                           /**< Role for item's rotation. Value of type qreal. */
        ItemFrameColorRole,                         /**< Role for item's frame color. Value of type QColor. */
        ItemFlipRole,                               /**< Role for item's flip state. Value of type bool. */

        ItemTimeRole,                               /**< Role for item's playback position, in milliseconds. Value of type qint64. Default value is -1. */
        ItemPausedRole,                             /**< Role for item's paused state. Value of type bool. */
        ItemSpeedRole,                              /**< Role for item's playback speed. Value of type qreal. */
        ItemSliderWindowRole,                       /**< Role for slider window that is displayed when the item is active. Value of type QnTimePeriod. */
        ItemSliderSelectionRole,                    /**< Role for slider selection that is displayed when the items is active. Value of type QnTimePeriod. */
        ItemCheckedButtonsRole,                     /**< Role for buttons that a checked in item's titlebar. Value of type int (QnResourceWidget::Buttons). */

        /* Context-based. */
        CurrentLayoutResourceRole,
        CurrentUserResourceRole,
        CurrentLayoutMediaItemsRole,
        CurrentMediaServerResourcesRole,

        /* Arguments. */
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
        EventTypeRole,                            /**< Role for business event type. Used in BusinessEventsLogAction. */


        /* Others. */
        HelpTopicIdRole,                            /**< Role for item's help topic. Value of type int. */
        PtzPresetRole,                              /**< Role for PTZ preset. Value of type QnPtzPreset. */
        TranslationRole,                            /**< Role for translations. Value of type QnTranslation. */

        ItemMouseCursorRole,                        /**< Role for item's mouse cursor. */
        DisplayHtmlRole                             /**< Same as Display role, but use HTML format. */
    };


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


/** Helper function to mark strings that are not to be translated. */
inline QString lit(const QByteArray &data) {
    return QLatin1String(data);
}

/** Helper function to mark strings that are not to be translated. */
inline QString lit(const char *s) {
    return QLatin1String(s);
}

/** Helper function to mark characters that are not to be translated. */
inline QChar lit(char c) {
    return QLatin1Char(c);
}


Q_DECLARE_METATYPE(Qn::TimePeriodTypes);
Q_DECLARE_METATYPE(Qn::TimePeriodType);
Q_DECLARE_METATYPE(Qn::TimePeriodContent);


#endif // QN_COMMON_GLOBALS_H
