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

    property real rightPadding: 0
    property real bottomPadding: 0

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
        anchors.fill: parent
        anchors.rightMargin: mainWindow.rightPadding
        anchors.bottomMargin: mainWindow.bottomPadding
        onCurrentItemChanged: sideNavigation.close()
    }

    UiController {}

    Loader { id: testLoader }

    Component.onCompleted:
    {
        updateNavigationBarPadding()

        var lastUsedUrl = getLastUsedUrl()

        if (lastUsedUrl)
        {
            lockScreenOrientation()
            Workflow.openResourcesScreen(getLastUsedSystemId())
            connectionManager.connectToServer(lastUsedUrl)
        }
        else if (!cloudStatusWatcher.cloudLogin)
        {
            Workflow.openCloudWelcomeScreen()
        }
        else
        {
            Workflow.openSessionsScreen()
        }

        if (initialTest)
            Workflow.startTest(initialTest)
    }

    Component.onDestruction:
    {
        connectionManager.disconnectFromServer(true)
    }

    Connections
    {
        target: Qt.inputMethod
        onKeyboardRectangleChanged: updateNavigationBarPadding()
    }

    Behavior on bottomPadding
    {
        enabled: Qt.platform.os == "android"
        NumberAnimation  { duration: 200; easing.type: Easing.InCubic  }
    }

    function updateNavigationBarPadding()
    {
        if (Qt.platform.os != "android")
            return

        var keyboardHeight = Qt.inputMethod.keyboardRectangle.height / Screen.devicePixelRatio

        if (getDeviceIsPhone() && Screen.primaryOrientation == Qt.LandscapeOrientation)
        {
            rightPadding = getNavigationBarHeight()
            bottomPadding = keyboardHeight
        }
        else
        {
            rightPadding = 0
            bottomPadding = getNavigationBarHeight() + keyboardHeight
        }
    }

    Screen.onPrimaryOrientationChanged: updateNavigationBarPadding()

    function lockScreenOrientation()
    {
        setScreenOrientation(Screen.orientation)
    }

    function unlockScreenOrientation()
    {
        setScreenOrientation(Qt.PrimaryOrientation)
    }
}
