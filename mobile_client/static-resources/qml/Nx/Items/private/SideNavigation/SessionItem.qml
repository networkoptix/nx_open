import QtQuick 2.0
import Nx 1.0
import com.networkoptix.qml 1.0

import "../../../../items/QnLoginPage.js" as LoginDialog
import "../../../../main.js" as Main
import "../../../../controls"

SideNavigationItem
{
    id: sessionItem

    property string sessionId
    property string systemName
    property string address
    property int port
    property string user
    property string password

    implicitWidth: 200
    implicitHeight: 72
    leftPadding: 16
    rightPadding: 16

    contentItem: Item
    {
        Column
        {
            width: parent.width
            anchors.verticalCenter: parent.verticalCenter

            Text
            {
                text: systemName
                font.pixelSize: 18
                font.weight: Font.DemiBold
                color: ColorTheme.windowText
                elide: Text.ElideRight
                width: parent.width - editButton.width
                height: 32
                verticalAlignment: Text.AlignVCenter

                QnIconButton
                {
                    id: editButton

                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.right
                    anchors.leftMargin: 8
                    icon: "/images/edit.png"

                    onClicked:
                    {
                        sideNavigation.close()
                        Main.openSavedSession(sessionId, address, port, user, password, systemName)
                    }
                }
            }

            Text
            {
                width: parent.width
                height: 24

                text: user
                font.pixelSize: 15
                font.weight: Font.DemiBold
                color: ColorTheme.contrast2
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
            }
        }
    }

    onClicked:
    {
        if (!active)
        {
            connectionManager.disconnectFromServer(false)
            mainWindow.currentSystemName = systemName
            LoginDialog.connectToServer(sessionId, address, port, user, password)
            if (stackView.depth > 1)
                Main.gotoResources()
        }
    }
}
