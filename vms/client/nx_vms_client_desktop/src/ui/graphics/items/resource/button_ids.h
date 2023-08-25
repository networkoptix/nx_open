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

    HotspotsButton = 1 << 4,
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

    // Indicates if is zoomed item, actual for cameras.
    ZoomStatusIconButton = 1 << 15,

    // Reload web page, web pages only.
    ReloadPageButton = 1 << 16,

    // Navigate back, web pages only.
    BackButton = 1 << 17,

    // Indicate if video stream is paused
    PauseButton = 1 << 18,

    //---------------------------------------------------------------------------------------------
    // Buttons on the playback panel.

    // Status of the recording, actual for cameras.
    RecordingStatusIconButton = 1 << 19,
};

} // namespace Qn
