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

    // Move to a dedicated window.
    DedicatedWindowButton = 1 << 2,

    // Show info, actual for all widgets.
    InfoButton = 1 << 3,

    // Rotate, actual for all widgets.
    RotateButton = 1 << 4,

    //---------------------------------------------------------------------------------------------
    // Media Widget specific buttons.

    HotspotsButton = 1 << 5,
    PtzButton = 1 << 6,
    FishEyeButton = 1 << 7,
    ZoomWindowButton = 1 << 8,
    IoModuleButton = 1 << 9,
    ScreenshotButton = 1 << 10,
    DbgScreenshotButton = 1 << 11,
    MotionSearchButton = 1 << 12,
    ObjectSearchButton = 1 << 13,

    //---------------------------------------------------------------------------------------------
    // Server statistics specific buttons.

    ShowLogButton = 1 << 14,
    CheckIssuesButton = 1 << 15,

    //---------------------------------------------------------------------------------------------
    // Buttons for the left panel, sorting left-to-right.

    // Indicates if is zoomed item, actual for cameras.
    ZoomStatusIconButton = 1 << 16,

    // Reload web page, web pages only.
    ReloadPageButton = 1 << 17,

    // Navigate back, web pages only.
    BackButton = 1 << 18,

    // Indicate if video stream is paused
    PauseButton = 1 << 19,

    //---------------------------------------------------------------------------------------------
    // Buttons on the playback panel.

    // Status of the recording, actual for cameras.
    RecordingStatusIconButton = 1 << 20,
};

} // namespace Qn
