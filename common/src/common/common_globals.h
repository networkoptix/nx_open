#ifndef QN_COMMON_GLOBALS_H
#define QN_COMMON_GLOBALS_H

#include <QtCore/QtGlobal>
#include <QtCore/QMetaType>

/**
 * Same as <tt>Q_GADGET</tt>, but doesn't trigger MOC, and can be used in namespaces.
 * The only sane use case is for generating metainformation for enums in
 * namespaces (e.g. for use with <tt>QnEnumNameMapper</tt>).
 */
#define Q_NAMESPACE                                                             \
    extern const QMetaObject staticMetaObject;                                  \
    extern const QMetaObject &getStaticMetaObject();                            \


namespace QnCommonGlobals {}
namespace Qn { using namespace QnCommonGlobals; }

#ifdef Q_MOC_RUN
class QnCommonGlobals
#else
namespace QnCommonGlobals
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

} // namespace QnCommonGlobals

#endif // QN_COMMON_GLOBALS_H
