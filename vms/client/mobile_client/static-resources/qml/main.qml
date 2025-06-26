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


    readonly property bool isPortraitLayout: width <= height

    visible: true
    color: ColorTheme.colors.dark4 ?? "transparent"

    function lp(path)
    {
        return "qrc:///" + path
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
        height: windowParams.availableHeight - screenNavigationBar.heightOffset

        function restoreActiveFocus()
        {
            if (activeFocusItem == Window.contentItem)
                Workflow.focusCurrentScreen()
        }

        onCurrentItemChanged:
        {
            mainWindow.color = currentItem.hasOwnProperty("backgroundColor")
                ? currentItem.backgroundColor
                : ColorTheme.colors.dark4
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
            && accountBelongsToOrganization
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
                return qsTr("Connection lost. Reconnecting...")
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

    NxObject
    {
        id: appGlobalState

        property var lastOpenedNodeId: NxGlobals.uuid("")
    }

    Android10BackGestureWorkaround
    {
        workaroundParentItem: contentItem
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
