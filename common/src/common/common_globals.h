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
    Q_FLAGS(CameraCapabilities)
public:
#else
    Q_NAMESPACE
#endif

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
    Q_DECLARE_FLAGS(CameraCapabilities, CameraCapability);
    Q_DECLARE_OPERATORS_FOR_FLAGS(CameraCapabilities);

    /**
     * \param capabilities              Camera capability flags containing some deprecated values.
     * \returns                         Camera capability flags with deprecated values replaced with new ones.
     */
    inline Qn::CameraCapabilities undeprecate(Qn::CameraCapabilities capabilities) {
        Qn::CameraCapabilities result = capabilities;

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
