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

    property real leftPadding: 0
    property real rightPadding: 0
    property real bottomPadding: 0
    readonly property real keyboardHeight: Qt.inputMethod.visible
        ? Qt.inputMethod.keyboardRectangle.height
            / (Qt.platform.os !== "ios" ? Screen.devicePixelRatio : 1)
        : 0

    readonly property real availableWidth: width - rightPadding - leftPadding
    readonly property real availableHeight: height - bottomPadding

    contentItem.x: leftPadding

    visible: true
    color: ColorTheme.windowBackground
    overlay.background: Rectangle
    {
        color: ColorTheme.transparent(ColorTheme.base5, 0.4)
        Behavior on opacity { NumberAnimation { duration: 200 } }
    }

    SideNavigation { id: sideNavigation }

    StackView
    {
        id: stackView

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

        onCurrentItemChanged: sideNavigation.close()
        onWidthChanged: autoScrollDelayTimer.restart()
        onHeightChanged: autoScrollDelayTimer.restart()
    }

    UiController {}

    Loader { id: testLoader }

    Component.onCompleted:
    {
        androidBarPositionWorkaround.tryUpdateBarPosition()

        if (autoLoginEnabled)
        {
            var url = getInitialUrl()
            var systemName = ""

            if (url == "")
            {
                url = getLastUsedUrl()
                systemName = getLastUsedSystemName()
            }

            if (url != "")
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
                rightPadding = 0
                leftPadding = 0
                bottomPadding = lastBarSize
            }
            else
            {
                leftPadding = lastLeftSideBar ? lastBarSize : 0
                rightPadding = lastLeftSideBar ? 0 : lastBarSize
                bottomPadding = 0
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
        onTriggered: Nx.ensureFlickableChildVisible(activeFocusItem)
    }
}
