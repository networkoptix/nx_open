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
    }

    SideNavigation { id: sideNavigation }

    StackView
    {
        id: stackView
        anchors.fill: parent
        anchors.rightMargin: rightPadding
        anchors.bottomMargin: bottomPadding
    }

    Component.onCompleted:
    {
        updateNavigationBarPadding()

        if (loginSessionManager.lastUsedSessionId)
        {
            lockScreenOrientation()
            Workflow.openResourcesScreen(loginSessionManager.lastUsedSessionSystemName())
            connectionManager.connectToServer(loginSessionManager.lastUsedSessionUrl())
        }
        else
        {
            Workflow.openSessionsScreen()
        }
    }

    Component.onDestruction:
    {
        connectionManager.disconnectFromServer(true)
    }

    function updateNavigationBarPadding()
    {
        if (Qt.platform.os != "android")
            return

        if (getDeviceIsPhone() && Screen.primaryOrientation == Qt.LandscapeOrientation)
        {
            rightPadding = getNavigationBarHeight()
            bottomPadding = 0
        }
        else
        {
            rightPadding = 0
            bottomPadding = getNavigationBarHeight()
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
