import QtQuick 2.0
import QtQuick.Window 2.2
import Qt.labs.controls 1.0
import Nx 1.0
import Nx.Controls 1.0
import Nx.Items 1.0
import com.networkoptix.qml 1.0

ApplicationWindow
{
    id: mainWindow

    property real leftPadding: leftCustomMargin
    property real rightPadding: rightCustomMargin
    property real bottomPadding: bottomCustomMargin

    readonly property real keyboardHeight: Qt.inputMethod.visible
        ? Qt.inputMethod.keyboardRectangle.height
            / (Qt.platform.os !== "ios" ? Screen.devicePixelRatio : 1)
        : 0

    readonly property real availableWidth: width - rightPadding - leftPadding
    readonly property real availableHeight: height - bottomPadding - topLevelWarning.height

    contentItem.x: leftPadding

    visible: true
    color: ColorTheme.windowBackground

    overlay.background: Rectangle
    {
        color: ColorTheme.transparent(ColorTheme.base5, 0.4)
        Behavior on opacity { NumberAnimation { duration: 200 } }
    }

    SideNavigation
    {
        id: sideNavigation

        y: topLevelWarning.height + deviceStatusBarHeight
    }

    StackView
    {
        id: stackView

        y: topLevelWarning.height
        width: mainWindow.availableWidth
        height: mainWindow.availableHeight - keyboardHeight

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
                : ColorTheme.windowBackground

            sideNavigation.close()
            updateCustomMargins()
        }
        onWidthChanged: autoScrollDelayTimer.restart()
        onHeightChanged: autoScrollDelayTimer.restart()
    }

    UiController {}

    Loader { id: testLoader }

    WarningPanel
    {
        id: topLevelWarning

        y: deviceStatusBarHeight
        x: -leftCustomMargin
        width: mainWindow.availableWidth + leftCustomMargin + rightCustomMargin
        text: d.warningText
        opened: text.length
    }

    Component.onCompleted:
    {
        androidBarPositionWorkaround.tryUpdateBarPosition()

        if (autoLoginEnabled)
        {
            var url = getInitialUrl()
            var systemName = ""

            if (url.isEmpty())
            {
                url = getLastUsedUrl()
                systemName = getLastUsedSystemName()
            }

            if (!url.isEmpty())
            {
                Workflow.openResourcesScreen(systemName)
                connectionManager.connectToServer(url)
                return
            }
        }

        // TODO: #dklychkov Check if need it in #3.1 and uncomment.
        // if (cloudStatusWatcher.status == QnCloudStatusWatcher.LoggedOut)
        // {
        //     Workflow.openCloudWelcomeScreen()
        //     return
        // }

        Workflow.openSessionsScreen()

        if (initialTest)
            Workflow.startTest(initialTest)
    }

    Connections
    {
        target: cloudStatusWatcher
        onStatusChanged:
        {
            if (isCloudConnectionUrl(getLastUsedUrl())
                && cloudStatusWatcher.status == QnCloudStatusWatcher.LoggedOut)
            {
                uiController.disconnectFromSystem()
            }
        }
    }

    Component.onDestruction:
    {
        connectionManager.disconnectFromServer()
    }

    Screen.onPrimaryOrientationChanged: androidBarPositionWorkaround.updateBarPosition()

    Timer
    {
        // We need periodically update paddings due to Qt does not emit signal
        // screenOrientationChanged when we change it from normal to inverted.

        // TODO: #ynikitenkov #future Check if we can get rid of navigation bar -related
        // properties and just use custom margins

        id: androidBarPositionWorkaround

        interval: 200
        repeat: true
        running: Qt.platform.os == "android"

        property int lastBarSize: -1
        property bool lastLeftSideBar: false

        onTriggered: tryUpdateBarPosition()

        function tryUpdateBarPosition()
        {
            if (Qt.platform.os != "android")
                return

            var barSize = getNavigationBarHeight()
            var leftSideBar = getNavigationBarIsLeftSide()

            if (barSize == lastBarSize && lastLeftSideBar == leftSideBar)
                return

            lastBarSize = barSize
            lastLeftSideBar = leftSideBar

            updateBarPosition()
        }

        function updateBarPosition()
        {
            if (Qt.platform.os != "android")
                return

            if (!getDeviceIsPhone() ||  Screen.primaryOrientation == Qt.PortraitOrientation)
            {
                rightPadding = rightCustomMargin
                leftPadding = leftCustomMargin
                bottomPadding = bottomCustomMargin + lastBarSize
            }
            else
            {
                leftPadding = leftCustomMargin + (lastLeftSideBar ? lastBarSize : 0)
                rightPadding = rightCustomMargin + (lastLeftSideBar ? 0 : lastBarSize)
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
        onTriggered: NxGlobals.ensureFlickableChildVisible(activeFocusItem)
    }

    Object
    {
        id: d

        readonly property bool cloudOffline: cloudStatusWatcher.status == QnCloudStatusWatcher.Offline

        property bool cloudOfflineDelayed: false
        property bool showCloudOfflineWarning:
            stackView.currentItem.objectName == "sessionsScreen" && cloudOfflineDelayed
        readonly property bool reconnecting: connectionManager.restoringConnection

        readonly property string warningText:
        {
            if (d.reconnecting)
                return qsTr("Server offline. Reconnecting...")
            return showCloudOfflineWarning
                ? qsTr("Cannot connect to %1").arg(applicationInfo.cloudName())
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
}
