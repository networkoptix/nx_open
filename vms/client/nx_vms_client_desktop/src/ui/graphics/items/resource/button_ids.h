#pragma once

namespace Qn {
// Constant value set order of the button
enum WidgetButtons
{
    //---------------------------------------------------------------------------------------------
    // Buttons for the right panel, sorting right-to-left.

    // Close widgets, actual for all widgets.
    CloseButton = 1 << 0,

    // Switch to fullscreen, web pages only.
    FullscreenButton = 1 << 1,

    // Show info, actual for all widgets.
    InfoButton = 1 << 2,

    // Rotate, actual for all widgets.
    RotateButton = 1 << 3,

    //---------------------------------------------------------------------------------------------
    // Media Widget specific buttons.

    ScreenshotButton = 1 << 4,
    MotionSearchButton = 1 << 5,
    PtzButton = 1 << 6,
    FishEyeButton = 1 << 7,
    ZoomWindowButton = 1 << 8,
    EnhancementButton = 1 << 9,
    DbgScreenshotButton = 1 << 10,
    IoModuleButton = 1 << 11,
    EntropixEnhancementButton = 1 << 12,

    //---------------------------------------------------------------------------------------------
    // Server statistics specific buttons.

    ShowLogButton = 1 << 13,
    CheckIssuesButton = 1 << 14,

    //---------------------------------------------------------------------------------------------
    // Buttons for the left panel, sorting left-to-right.

    // Status of the recording, actual for cameras.
    RecordingStatusIconButton = 1 << 15,

    // Reload web page, web pages only.
    ReloadPageButton = 1 << 16,

    // Navigate back, web pages only.
    BackButton = 1 << 17,
};

} // namespace Qn
