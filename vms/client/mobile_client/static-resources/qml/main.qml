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


    readonly property bool isPortraitLayout: width <= height

    visible: true
    color: ColorTheme.colors.windowBackground ?? "transparent"

    function lp(path)
    {
        return "qrc://" + path
    }

    // Workaround object for Qt bug. On iOS main window padding (which should be equal to
    // SafeArea.margins) are not updated, but SafeArea works as expected.
    QtObject
    {
        id: windowParams

        readonly property int topMargin: mainWindow.SafeArea.margins.top
        readonly property int bottomMargin: mainWindow.SafeArea.margins.bottom
        readonly property int leftMargin: mainWindow.SafeArea.margins.left
        readonly property int rightMargin: mainWindow.SafeArea.margins.right

        readonly property real availableWidth: width - windowParams.leftMargin - windowParams.rightMargin
        readonly property real availableHeight: height
            - topLevelWarning.height
            - windowParams.topMargin
            - windowParams.bottomMargin
    }

    // For consistency explicitly set all paddings for the main window to our windowParams values.
    Binding { target: mainWindow; property: "topPadding"; value: windowParams.topMargin }
    Binding { target: mainWindow; property: "bottomPadding"; value: windowParams.bottomMargin }
    Binding { target: mainWindow; property: "leftPadding"; value: windowParams.leftMargin }
    Binding { target: mainWindow; property: "rightPadding"; value: windowParams.rightMargin }

    StackView
    {
        id: stackView

        objectName: "mainStackView"

        y: topLevelWarning.height
        width: windowParams.availableWidth
        height: windowParams.availableHeight - keyboardHeight - screenNavigationBar.heightOffset

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

    WarningPanel
    {
        id: topLevelWarning

        x: -windowParams.leftMargin
        width: parent.width
        text: d.warningText
        opened: text.length
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
