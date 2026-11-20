// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQml

import Nx.Mobile

/**
 * Mock of the application window context, available in QML as `windowContext`.
 *
 * Has only what the tested components use. A new member must have the name and the type it has in
 * the application (checked by tst_WindowContextMock.qml), a writable one is restored in reset().
 */
QtObject
{
    readonly property QtObject deprecatedUiController: QtObject
    {
        property int currentScreen: Controller.UnknownScreen
    }

    readonly property QtObject ui: QtObject
    {
        readonly property QtObject measurements: QtObject
        {
            property int androidKeyboardHeight: 0
            property int deviceStatusBarHeight: 0
        }

        // Functions only report the call, for SignalSpy.
        readonly property QtObject windowHelpers: QtObject
        {
            signal enterFullscreenCalled()
            signal exitFullscreenCalled()
            signal setScreenOrientationCalled(int orientation)
            signal releaseScreenOrientationCalled(int orientation)

            function enterFullscreen()
            {
                enterFullscreenCalled()
            }

            function exitFullscreen()
            {
                exitFullscreenCalled()
            }

            function setScreenOrientation(orientation)
            {
                setScreenOrientationCalled(orientation)
            }

            function releaseScreenOrientation(orientation)
            {
                releaseScreenOrientationCalled(orientation)
            }
        }
    }

    function reset()
    {
        deprecatedUiController.currentScreen = Controller.UnknownScreen
        ui.measurements.androidKeyboardHeight = 0
        ui.measurements.deviceStatusBarHeight = 0
    }
}
