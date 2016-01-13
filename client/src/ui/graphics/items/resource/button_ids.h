
#pragma once

namespace Qn
{
    enum RightPanelButtonId
    {
        // QnResourceWidget
        kCloseButton                = 0x00001
        , kInfoButton               = 0x00004
        , kRotateButton             = 0x00008

        // QnWebResourceWidget
        , kFullscreenButton         = 0x00002

        // QnMediaResourceWidget
        , kScreenshotButton         = 0x00010
        , kMotionSearchButton       = 0x00020
        , kPtzButton                = 0x00040
        , kFishEyeButton            = 0x00080
        , kZoomWindowButton         = 0x00100
        , kEnhancementButton        = 0x00200
        , kDbgScreenshotButton      = 0x00400
        , kIoModuleButton           = 0x00800

        // QnServerResourceWidget
        , kPingButton               = 0x01000
        , kShowLogButton            = 0x02000
        , kCheckIssuesButton        = 0x04000

    };

    enum LeftPanelButtonId
    {
        // QnResourceWidget
        kRecordingStatusIconButton  = 0x08000

        // QnWebResourceWidget
        , kReloadPageButton         = 0x10000
        , kBackButton               = 0x20000
    };
}
