// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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

    PtzButton = 1 << 4,
    FishEyeButton = 1 << 5,
    ZoomWindowButton = 1 << 6,
    IoModuleButton = 1 << 7,
    ScreenshotButton = 1 << 8,
    DbgScreenshotButton = 1 << 9,
    MotionSearchButton = 1 << 10,
    ObjectSearchButton = 1 << 11,

    //---------------------------------------------------------------------------------------------
    // Server statistics specific buttons.

    ShowLogButton = 1 << 12,
    CheckIssuesButton = 1 << 13,

    //---------------------------------------------------------------------------------------------
    // Buttons for the left panel, sorting left-to-right.

    // Status of the recording, actual for cameras.
    RecordingStatusIconButton = 1 << 14,

    // Reload web page, web pages only.
    ReloadPageButton = 1 << 15,

    // Navigate back, web pages only.
    BackButton = 1 << 16,

    // Indicate if video stream is paused
    PauseButton = 1 << 17,
};

} // namespace Qn
