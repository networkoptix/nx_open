// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

pragma Singleton

import QtQml
import QtQuick.Controls

import Nx.Core
import Nx.Mobile

NxObject
{
    id: layoutController

    property ApplicationWindow mainWindow
    property StackView stackView: mainWindow?.uiContainer.stackView ?? null //< Makes proper context for the Workflow.

    // Current display orientation.
    readonly property bool isPortrait: mainWindow ? mainWindow.width <= mainWindow.height : false

    // TODO: Now `isMobile` and `isTablet` properties uses for the controls customization.
    // `isMobile` means a small screen, `isTablet` means a large screen. This properties must be
    // moved to some ScreenSizeController.
    readonly property bool isMobile: !isTablet
    // Whether the given device is a tablet.
    readonly property bool isTablet: appContext.settings.forceTabletMode //< TODO: introduce enum for a device type.

    readonly property bool isTabletLayout: appContext.settings.forceTabletMode && !isPortrait
    onIsTabletLayoutChanged:
    {
        if (!isTabletLayout)
            return

        if (windowContext.deprecatedUiController.currentScreen === Controller.MenuScreen)
        {
            // Menu screen in not enabled on the tablet layout, open default one.
            Workflow.openDefaultScreen()
            return
        }
    }

    readonly property alias fullscreen: d.fullscreen

    function enterFullscreen(orientation = Qt.LandscapeOrientation)
    {
        if (d.fullscreen)
            return

        d.setFullscreen(true)

        // Tablets are never rotated programmatically: the video simply fills the screen in
        // the current orientation, as the native players do. Also, iPadOS 26+ ignores such
        // requests altogether - it resizes the app window instead of rotating the device.
        if (isTablet)
            return

        if (isPortrait === (orientation === Qt.PortraitOrientation))
            return //< Already in the wanted orientation, nothing to force.

        d.orientationToRestore = isPortrait ? Qt.PortraitOrientation : Qt.LandscapeOrientation

        if (CoreUtils.isMobilePlatform())
            windowContext.ui.windowHelpers.setScreenOrientation(orientation)
        else
            d.swapWindowSize()
    }

    function exitFullscreen()
    {
        if (!d.fullscreen)
            return

        d.setFullscreen(false)

        const orientation = d.orientationToRestore
        d.orientationToRestore = 0

        if (!orientation || isPortrait === (orientation === Qt.PortraitOrientation))
            return

        if (CoreUtils.isMobilePlatform())
            windowContext.ui.windowHelpers.setScreenOrientation(orientation)
        else
            d.swapWindowSize()
    }

    function toggleFullscreen(orientation = Qt.LandscapeOrientation)
    {
        if (d.fullscreen)
            exitFullscreen()
        else
            enterFullscreen(orientation)
    }

    Connections
    {
        target: layoutController.stackView

        function onCurrentItemChanged()
        {
            // Any screen change (push/pop/replace) leaves the fullscreen mode: the global state
            // must not leak to another screen, and the orientation lock must be restored.
            layoutController.exitFullscreen()
        }
    }

    QtObject
    {
        id: d

        property bool fullscreen: false

        // The orientation to restore on exitFullscreen() when enterFullscreen() forced it
        // away; 0 while nothing is forced.
        property int orientationToRestore: 0

        function setFullscreen(value)
        {
            if (fullscreen === value)
                return

            fullscreen = value

            // The system status bar is hidden for the whole fullscreen session.
            if (value)
                windowContext.ui.windowHelpers.enterFullscreen()
            else
                windowContext.ui.windowHelpers.exitFullscreen()
        }

        function swapWindowSize()
        {
            const window = layoutController.mainWindow;
            [window.width, window.height] = [window.height, window.width]
        }
    }
}
