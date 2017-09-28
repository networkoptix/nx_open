import QtQuick 2.0
import Qt.labs.controls 1.0
import Nx 1.0
import com.networkoptix.qml 1.0

Page
{
    id: page

    property bool activePage: StackView.status === StackView.Active
    property int pageStatus: StackView.status
    property bool sideNavigationEnabled: true
    property var screenOrientation: Qt.PrimaryOrientation

    background: Rectangle { color: ColorTheme.windowBackground }

    onSideNavigationEnabledChanged: updateSideNavigation()
    onActivePageChanged:
    {
        if (activePage)
        {
            updateSideNavigation()
            setScreenOrientation(screenOrientation)
        }
    }

    Keys.onPressed:
    {
        if (Utils.keyIsBack(event.key))
        {
            if (sideNavigation.opened)
                sideNavigation.close()
            else if (isConnecting())
                uiController.disconnectFromSystem()
            else if (stackView.depth > 1)
                Workflow.popCurrentScreen()
            else if (event.key != Qt.Key_Escape)
                quitApplication()
            else
                return

            event.accepted = true
        }
        else if (event.key == Qt.Key_F2)
        {
            if (sideNavigationEnabled)
                sideNavigation.open()

            event.accepted = true
        }
    }

    function isConnecting()
    {
        var state = connectionManager.connectionState
        return state == QnConnectionManager.Connecting
            || (state == QnConnectionManager.Connected && !connectionManager.online)
    }
    function updateSideNavigation()
    {
        if (!sideNavigationEnabled)
            sideNavigation.close()

        sideNavigation.enabled = sideNavigationEnabled
    }
}
