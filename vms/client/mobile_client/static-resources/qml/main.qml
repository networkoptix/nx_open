// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls as Controls

import Nx.Controls
import Nx.Core
import Nx.Mobile
import Nx.Screens
import Nx.Ui

import nx.vms.client.core

Controls.ApplicationWindow
{
    id: mainWindow

    objectName: "mainWindow"

    readonly property bool hasNavigationBar:
        !!windowContext.ui.windowHelpers.navigationBarSize()

    property alias windowParams: windowParams
    property alias uiContainer: uiContainer

    visible: true
    background: Rectangle { color: ColorTheme.colors.dark7 }

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
            - windowParams.topMargin
            - windowParams.bottomMargin
    }

    // For consistency explicitly set all paddings for the main window to our windowParams values.
    Binding { target: mainWindow; property: "topPadding"; value: windowParams.topMargin }
    Binding { target: mainWindow; property: "bottomPadding"; value: windowParams.bottomMargin }
    Binding { target: mainWindow; property: "leftPadding"; value: windowParams.leftMargin }
    Binding { target: mainWindow; property: "rightPadding"; value: windowParams.rightMargin }

    UiContainer
    {
        id: uiContainer

        anchors.fill: parent
    }

    SnackBar
    {
        id: snackBar

        parent: Controls.Overlay.overlay
        y: LayoutController.isTabletLayout
            ? uiContainer.navigationBar.y - height - 16
            : mainWindow.height - height - 16
    }

    onActiveFocusItemChanged:
    {
        autoScrollDelayTimer.restart()
        uiContainer.stackView.restoreActiveFocus()
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
        property bool showCloudOfflineWarning: uiContainer.stackView.currentItem
            && uiContainer.stackView.currentItem.objectName === "sessionsScreen"
            && cloudOfflineDelayed

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

    onClosing:
        (close) =>
        {
            // To make sure that we have the correct order of deinitialization we
            // handle window closing manually.
            close.accepted = false

            mainWindow.hide()

            Qt.callLater(() => appContext.closeWindow())
        }

    Component.onCompleted:
    {
        LayoutController.mainWindow = mainWindow

        let startupHandler =
            function()
            {
                if (windowContext.ui.tryRestoreLastUsedConnection())
                    return

                if (windowContext.deprecatedUiController.currentScreen === Controller.UnknownScreen)
                    windowContext.deprecatedUiController.openSessionsScreen()
            };

        CoreUtils.executeLater(startupHandler, this)
    }
}
