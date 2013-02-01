#ifndef QN_COMMON_GLOBALS_H
#define QN_COMMON_GLOBALS_H

#include <QtCore/QtGlobal>

#include <utils/common/gadget.h>

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
    QN_GADGET
#endif

    enum ExtrapolationMode {
        ConstantExtrapolation,
        LinearExtrapolation,
        PeriodicExtrapolation
    };

    enum CameraCapability { 
        NoCapabilities                      = 0x0, 
        //ContinuousPtzCapability             = 0x01, 
        ZoomCapability                      = 0x02, 
        PrimaryStreamSoftMotionCapability   = 0x04,
        relayInput                          = 0x08,
        relayOutput                         = 0x10,
        AbsolutePtzCapability               = 0x20,
        ContinuousPanTiltCapability         = 0x40,
        ContinuousZoomCapability            = 0x80,

        /* Shortcuts */
        AllPtzCapabilities                  = AbsolutePtzCapability | ContinuousPanTiltCapability | ContinuousZoomCapability,

        /* Deprecated capabilities. */
        DeprecatedContinuousPtzCapability   = 0x01,
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
