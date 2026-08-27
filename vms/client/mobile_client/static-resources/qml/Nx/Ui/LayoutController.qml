// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

pragma Singleton

import QtQml
import QtQuick.Controls

import Nx.Core
import Nx.Mobile

NxObject
{
    id: layoutController

    // Window height below which the bottom navigation bar and the bottom sheets do not fit. Below
    // this threshold, HorizontalCompact mode is used.
    readonly property int shortWindowHeight: 480

    // Window width starting from which the side panels fit next to the content. Below this
    // threshold, Compact mode is used; above it, Medium mode is used.
    readonly property int mediumWindowWidth: 670

    // Window width starting from which the content pane is wide enough for the large metrics. Below
    // this threshold, Medium mode is used; above it, Expanded mode is used.
    readonly property int expandedWindowWidth: 840

    // Whether the window is taller than wide. This is plain window geometry, NOT a layout mode:
    // use it only where the shape of the window itself matters - the fullscreen video controls,
    // the system gesture exclusion areas - and never to decide a layout.
    readonly property bool isPortrait: mainWindow
        ? mainWindow.width <= mainWindow.height
        : false

    property ApplicationWindow mainWindow
    property StackView stackView: mainWindow?.uiContainer.stackView ?? null //< Makes proper context for the Workflow.

    readonly property int mode:
    {
        const forcedMode = appContext.settings.forcedLayoutMode
        if (forcedMode >= 0)
            return forcedMode

        if (!mainWindow)
            return LayoutMode.Compact

        if (mainWindow.height < layoutController.shortWindowHeight)
            return LayoutMode.HorizontalCompact

        if (mainWindow.width < layoutController.mediumWindowWidth)
            return LayoutMode.Compact

        return mainWindow.width < layoutController.expandedWindowWidth
            ? LayoutMode.Medium
            : LayoutMode.Expanded
    }

    readonly property string modeName:
    {
        switch (mode)
        {
            case LayoutMode.HorizontalCompact:
                return "horizontal compact"
            case LayoutMode.Compact:
                return "compact"
            case LayoutMode.Medium:
                return "medium"
            case LayoutMode.Expanded:
                return "expanded"
        }

        return "unknown"
    }

    readonly property bool isCompact: mode === LayoutMode.Compact
    readonly property bool isHorizontalCompact: mode === LayoutMode.HorizontalCompact

    // Whether the screen is split into a content area with the optional side panels around it.
    // Such a layout also moves the navigation into a rail at the left edge, freeing the bottom.
    readonly property bool hasSidePanels:
        mode === LayoutMode.Medium || mode === LayoutMode.Expanded

    readonly property bool isExpanded: mode === LayoutMode.Expanded

    onHasSidePanelsChanged:
    {
        if (!hasSidePanels)
            return

        if (windowContext.deprecatedUiController.currentScreen === Controller.MenuScreen)
        {
            // Menu screen is not reachable from the navigation rail, open the default one.
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

        // Tablet-sized layouts are never rotated programmatically: the video simply fills the
        // screen in the current orientation, as the native players do. Also, iPadOS 26+ ignores
        // such requests altogether - it resizes the app window instead of rotating the device.
        if (hasSidePanels)
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
