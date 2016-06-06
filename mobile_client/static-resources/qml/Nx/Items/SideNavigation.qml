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

    readonly property bool opened: position > 0

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
                    visible: connectionManager.online
                }
            }
        }

        OfflineDummy
        {
            anchors.fill: savedSessionsList
            anchors.margins: 16
            visible: !connectionManager.online
        }

        Column
        {
            id: bottomContent

            width: parent.width
            anchors.bottom: parent.bottom

            BottomSeparator
            {
                visible: connectionManager.online
            }

            SideNavigationButton
            {
                icon: lp("/images/plus.png")
                text: qsTr("New connection")
                visible: !connectionManager.online
                onClicked:
                {
                    sideNavigation.close()
                    Workflow.openNewSessionScreen()
                }
            }

            SideNavigationButton
            {
                id: disconnectButton

                icon: lp("/images/disconnect.png")
                text: qsTr("Disconnect from server")
                visible: connectionManager.online ||
                         connectionManager.connectionState == QnConnectionManager.Connecting
                onClicked:
                {
                    clearLastUsedConnection()
                    sideNavigation.close()
                    connectionManager.disconnectFromServer(false)
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

    onOpenedChanged:
    {
        if (opened && liteMode)
        {
            // TODO: #dklychkov Implement proper focus control
            disconnectButton.forceActiveFocus()
        }
    }

    Keys.onPressed:
    {
        if (Utils.keyIsBack(event.key))
        {
            close()
            Workflow.focusCurrentScreen()
        }
    }
}
