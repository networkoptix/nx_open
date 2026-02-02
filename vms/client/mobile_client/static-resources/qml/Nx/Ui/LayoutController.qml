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
    property StackView stackView: mainWindow.uiContainer.stackView //< Makes proper context for the Workflow.

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
}
