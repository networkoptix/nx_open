#ifndef QN_COMMON_GLOBALS_H
#define QN_COMMON_GLOBALS_H

#include <QtCore/QtGlobal>

namespace Qn {
    enum CameraCapability { 
        NoCapabilities                      = 0x0, 
        ContinuousPtzCapability             = 0x01, 
        ZoomCapability                      = 0x02, 
        PrimaryStreamSoftMotionCapability   = 0x04,
        relayInput                          = 0x08,
        relayOutput                         = 0x10,
        AbsolutePtzCapability               = 0x20
    };
    Q_DECLARE_FLAGS(CameraCapabilities, CameraCapability);

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

} // namespace Qn

Q_DECLARE_OPERATORS_FOR_FLAGS(Qn::CameraCapabilities);







#endif // QN_COMMON_GLOBALS_H
