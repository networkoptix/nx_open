
#pragma once

namespace Qn
{
    // @note Constant value set order of the button
    enum WidgetButtons
    {
        // Buttons for right panel

        // QnResourceWidget
        CloseButton                 = 0x00001
        , InfoButton                = 0x00004
        , RotateButton              = 0x00008

        // QnWebResourceWidget
        , FullscreenButton          = 0x00002

        // QnMediaResourceWidget
        , ScreenshotButton          = 0x00010
        , MotionSearchButton        = 0x00020
        , PtzButton                 = 0x00040
        , FishEyeButton             = 0x00080
        , ZoomWindowButton          = 0x00100
        , EnhancementButton         = 0x00200
        , DbgScreenshotButton       = 0x00400
        , IoModuleButton            = 0x00800
        , AnalyticsButton           = 0x01000
        , EntropixEnhancementButton = 0x02000

        // QnServerResourceWidget
        , ShowLogButton             = 0x04000
        , CheckIssuesButton         = 0x08000

        // Buttons for left panel

        // QnResourceWidget
        , RecordingStatusIconButton = 0x10000

        // QnWebResourceWidget
        , ReloadPageButton          = 0x20000
        , BackButton                = 0x40000
    };
}
