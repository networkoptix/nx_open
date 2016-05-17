import QtQuick 2.0
import Qt.labs.controls 1.0
import Nx 1.0
import com.networkoptix.qml 1.0

import "private/SideNavigation"

Drawer
{
    id: sideNavigation

    position: 0
    enabled: stackView.depth == 1

    Rectangle
    {
        width: Math.min(ApplicationWindow.window.width - 56, ApplicationWindow.window.height - 56, 56 * 6)
        height: ApplicationWindow.window.height
        color: ColorTheme.base8
        clip: true

        ListView
        {
            id: savedSessionsList

            anchors.fill: parent
            anchors.topMargin: getStatusBarHeight()
            anchors.bottomMargin: bottomContent.height
            flickableDirection: Flickable.VerticalFlick
            clip: true

            header: Column
            {
                width: savedSessionsList.width

                CloudPanel {}

                SystemInformationPanel
                {
                    visible: connectionManager.isOnline
                }
            }
        }

        OfflineDummy
        {
            anchors.fill: savedSessionsList
            anchors.margins: 16
            visible: !connectionManager.isOnline
        }

        Column
        {
            id: bottomContent

            width: parent.width
            anchors.bottom: parent.bottom

            BottomSeparator
            {
                visible: connectionManager.isOnline
            }

            SideNavigationButton
            {
                icon: lp("/images/plus.png")
                text: qsTr("New connection")
                visible: !currentSessionId
                onClicked:
                {
                    sideNavigation.close()
                    Workflow.openNewSessionScreen()
                }
            }

            SideNavigationButton
            {
                icon: lp("/images/close.png")
                text: qsTr("Disconnect from server")
                visible: currentSessionId
                onClicked:
                {
                    sideNavigation.close()
                    connectionManager.disconnectFromServer(false)
                    currentSessionId = ""
                    Workflow.openSessionsScreen()
                }
            }

            SideNavigationButton
            {
                icon: lp("/images/settings.png")
                text: qsTr("Settings")
                visible: !liteMode
                onClicked: Workflow.openSettingsScreen()
            }

            VersionLabel {}
        }
    }
}
