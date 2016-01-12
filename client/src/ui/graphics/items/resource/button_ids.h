
#pragma once

namespace Qn
{
    enum RightPanelButtonId
    {
        // QnResourceWidget
        kCloseButton                = 0x0001
        , kInfoButton               = 0x0004
        , kRotateButton             = 0x0008

        // QnWebResourceWidget
        , kFullscreenButton         = 0x0002

        // QnMediaResourceWidget
        , kScreenshotButton         = 0x0010
        , kMotionSearchButton       = 0x0020
        , kPtzButton                = 0x0040
        , kFishEyeButton            = 0x0080
        , kZoomWindowButton         = 0x0100
        , kEnhancementButton        = 0x0200
        , kDbgScreenshotButton      = 0x0400
        , kIoModuleButton           = 0x0800

        // QnServerResourceWidget
        , kPingButton               = 0x1000
        , kShowLogButton            = 0x2000
        , kCheckIssuesButton        = 0x4000

    };

    enum LeftPanelButtonId
    {
        // QnResourceWidget
        kRecordingStatusIconButton  = 0x0001

        // QnWebResourceWidget
        , kReloadPageButton         = 0x0002
        , kBackButton               = 0x0004
    };
}
