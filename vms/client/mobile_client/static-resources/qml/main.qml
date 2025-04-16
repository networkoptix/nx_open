// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Window
import QtQuick.Controls as Controls

import Nx.Core
import Nx.Controls
import Nx.Items
import Nx.Mobile
import Nx.Ui

import nx.vms.client.core
import nx.vms.client.mobile

Controls.ApplicationWindow
{
    id: mainWindow

    readonly property var customMargins: windowContext.ui.measurements.customMargins
    leftPadding: customMargins.left
    rightPadding: customMargins.right
    bottomPadding: customMargins.bottom

    readonly property bool hasNavigationBar:
        !!windowContext.ui.windowHelpers.navigationBarSize()

    // Qt bug workaround. For some reason on the new devices with iOS 12+
    // Qt.inputMethod.visible hangs with wrong value. It is "true" even if
    // keyboard is hidden. But Qt.inputMethod.anchorRectangle always has corret value
    // and is presented if keyboard is visible (otherwise it is empty).
    // So we use it as workaround in the iOS and check with "visible" property together.
    readonly property bool inputHasAnchorRectangle: Qt.platform.os === "ios"
        && (Qt.inputMethod.anchorRectangle.width != 0 || Qt.inputMethod.anchorRectangle.height != 0)
    readonly property real keyboardHeight: Qt.inputMethod.visible && inputHasAnchorRectangle
        ? Qt.inputMethod.keyboardRectangle.height
            / (Qt.platform.os !== "ios" ? Screen.devicePixelRatio : 1)
        : 0

    readonly property real availableWidth: width - rightPadding - leftPadding
    readonly property real availableHeight: height - bottomPadding - topLevelWarning.height

    readonly property bool isPortraitLayout: width <= height

    contentItem.x: leftPadding

    visible: true
    color: ColorTheme.colors.windowBackground ?? "transparent"

    function lp(path)
    {
        return "qrc://" + path
    }

    SideNavigation
    {
        id: sideNavigation

        y: topLevelWarning.height + windowContext.ui.measurements.deviceStatusBarHeight
    }

    StackView
    {
        id: stackView

        objectName: "mainStackView"

        y: topLevelWarning.height
        width: mainWindow.availableWidth
        height: mainWindow.availableHeight - keyboardHeight - screenNavigationBar.heightOffset

        property real keyboardHeight: mainWindow.keyboardHeight
        Behavior on keyboardHeight
        {
            enabled: Qt.platform.os === "android"
            NumberAnimation { duration: 200; easing.type: Easing.InCubic }
        }

        function restoreActiveFocus()
        {
            if (activeFocusItem == Window.contentItem)
                Workflow.focusCurrentScreen()
        }

        onCurrentItemChanged:
        {
            mainWindow.color = currentItem.hasOwnProperty("backgroundColor")
                ? currentItem.backgroundColor
                : ColorTheme.colors.windowBackground

            sideNavigation.close()
            windowContext.ui.measurements.updateCustomMargins()
        }
        onWidthChanged: autoScrollDelayTimer.restart()
        onHeightChanged: autoScrollDelayTimer.restart()
    }

    ScreenNavigationBar
    {
        id: screenNavigationBar
    }

    UiController
    {
        stackView: stackView
    }

    Loader { id: testLoader }

    WarningPanel
    {
        id: topLevelWarning

        y: windowContext.ui.measurements.deviceStatusBarHeight
        x: -customMargins.left
        width: mainWindow.availableWidth
            + customMargins.left
            + customMargins.right
        text: d.warningText
        opened: text.length
    }

    onSceneGraphInitialized: windowContext.ui.measurements.updateCustomMargins()

    Screen.onPrimaryOrientationChanged: androidBarPositionWorkaround.updateBarPosition()

    Timer
    {
        // We need periodically update paddings due to Qt does not emit signal
        // screenOrientationChanged when we change it from normal to inverted.

        // TODO: #ynikitenkov #future Check if we can get rid of navigation bar-related
        // properties and just use custom margins.
        // TODO: Update: use custom margins from insets when we drop android until 7.0. Take into
        // consderation modern android cutout modes.

        id: androidBarPositionWorkaround

        interval: 200
        repeat: true
        running: Qt.platform.os == "android"

        property int lastBarSize: -1
        property int lastBarType: windowContext.ui.windowHelpers.navigationBarType()

        onTriggered: tryUpdateBarPosition()

        function tryUpdateBarPosition()
        {
            if (Qt.platform.os != "android")
                return

            var barSize = windowContext.ui.windowHelpers.navigationBarSize()
            var barType = windowContext.ui.windowHelpers.navigationBarType()

            if (barSize == lastBarSize && lastBarType == barType)
                return

            lastBarSize = barSize
            lastBarType = barType

            updateBarPosition()
        }

        function updateBarPosition()
        {
            if (Qt.platform.os != "android")
                return

            // TODO: #future #ynikitenkov Check if we can get rid of check for tablet device type.
            if (!windowContext.ui.windowHelpers.isRunOnPhone()
                || lastBarType == NavigationBar.BottomNavigationBar)
            {
                rightPadding = customMargins.right
                leftPadding = customMargins.left
                bottomPadding = customMargins.bottom + lastBarSize
            }
            else
            {
                var leftSideBar = lastBarType == NavigationBar.LeftNavigationBar
                leftPadding = leftCustomMargin + (leftSideBar ? lastBarSize : 0)
                rightPadding = rightCustomMargin + (leftSideBar ? 0 : lastBarSize)
                bottomPadding = bottomCustomMargin
            }
        }
    }


    onActiveFocusItemChanged:
    {
        autoScrollDelayTimer.restart()
        stackView.restoreActiveFocus()
    }

    Timer
    {
        id: autoScrollDelayTimer
        interval: 50
        onTriggered:
        {
            if (activeFocusItem && !NxGlobals.ensureFlickableChildVisible(activeFocusItem))
                NxGlobals.ensureFlickableChildVisible(activeFocusItem.placeholder)
        }
    }

    CloudUserProfileWatcher
    {
        id: cloudUserProfileWatcher
        statusWatcher: appContext.cloudStatusWatcher
        readonly property bool isOrgUser:
            appContext.cloudStatusWatcher.status != CloudStatusWatcher.LoggedOut
            && channelPartnerList && channelPartnerList.length
    }

    NxObject
    {
        id: d

        readonly property bool cloudOffline:
            appContext.cloudStatusWatcher.status === CloudStatusWatcher.Offline

        property bool cloudOfflineDelayed: false
        property bool showCloudOfflineWarning:
            stackView.currentItem && stackView.currentItem.objectName === "sessionsScreen" && cloudOfflineDelayed

        readonly property string warningText:
        {
            if (windowContext.sessionManager.hasReconnectingSession)
                return qsTr("Server offline. Reconnecting...")
            return showCloudOfflineWarning
                ? qsTr("Cannot connect to %1", "%1 is the short cloud name (like 'Cloud')")
                    .arg(appContext.appInfo.cloudName())
                : ""
        }

        onCloudOfflineChanged:
        {
            if (cloudOffline)
            {
                cloudWarningDelayTimer.restart()
                return
            }

            cloudWarningDelayTimer.stop()
            cloudOfflineDelayed = false
        }

        Timer
        {
            id: cloudWarningDelayTimer
            interval: 5000
            onTriggered: d.cloudOfflineDelayed = true
        }
    }

    Android10BackGestureWorkaround
    {
        workaroundParentItem: contentItem
    }

    function setupColorTheme()
    {
        ColorTheme.colors["windowBackground"] = ColorTheme.colors.dark4
        ColorTheme.colors["backgroundDimColor"] = ColorTheme.transparent(ColorTheme.colors.dark5, 0.4)
        ColorTheme.windowText = ColorTheme.colors.light1
    }

    onClosing:
        (close) =>
        {
            // To make sure that we have the correct order of deinitialization we
            // handle window closing manually.
            close.accepted = false
            appContext.closeWindow()
        }

    Component.onCompleted:
    {
        setupColorTheme()

        windowContext.ui.measurements.updateCustomMargins()
        androidBarPositionWorkaround.tryUpdateBarPosition()

        let startupHandler =
            function()
            {
                if (windowContext.ui.tryRestoreLastUsedConnection())
                    return

                if (windowContext.depricatedUiController.currentScreen === Controller.UnknownScreen)
                    windowContext.depricatedUiController.openSessionsScreen()
            };

        CoreUtils.executeLater(startupHandler, this)
    }
}
