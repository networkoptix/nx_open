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

    // Mute button, available only when "play audio from all cameras" option is ON.
    MuteButton = 1 << 3,

    // Rotate, actual for all widgets.
    RotateButton = 1 << 4,

    //---------------------------------------------------------------------------------------------
    // Media Widget specific buttons.

    PtzButton = 1 << 5,
    FishEyeButton = 1 << 6,
    ZoomWindowButton = 1 << 7,
    IoModuleButton = 1 << 8,
    ScreenshotButton = 1 << 9,
    DbgScreenshotButton = 1 << 10,
    MotionSearchButton = 1 << 11,
    ObjectSearchButton = 1 << 12,

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
