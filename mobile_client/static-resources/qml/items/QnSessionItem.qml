import QtQuick 2.4

import com.networkoptix.qml 1.0

import "QnLoginPage.js" as LoginDialog
import "../main.js" as Main
import "../controls"

QnSideNavigationItem {
    id: sessionItem

    property string sessionId
    property string systemName
    property string address
    property int port
    property string user
    property string password

    width: parent.width
    height: dp(104)

    Column {
        anchors.verticalCenter: parent.verticalCenter
        x: dp(16)
        width: parent.width - 2 * x

        Text {
            id: systemNameLabel
            text: systemName
            font.pixelSize: dp(18)
            font.weight: Font.DemiBold
            color: QnTheme.listText
            elide: Text.ElideRight
            width: parent.width - editButton.width
            height: dp(32)
            verticalAlignment: Text.AlignVCenter

            QnIconButton {
                id: editButton

                icon: "image://icon/edit.png"

                anchors {
                    verticalCenter: parent.verticalCenter
                    left: parent.right
                }

                onClicked: Main.openSavedSession(sessionId, address, port, user, password, systemName)
            }
        }

        Text {
            text: address
            font.pixelSize: dp(15)
            font.weight: Font.DemiBold
            color: QnTheme.listSubText
            width: parent.width
            height: dp(24)
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        Text {
            text: user
            font.pixelSize: dp(15)
            font.weight: Font.DemiBold
            color: QnTheme.listSubText
            elide: Text.ElideRight
            width: parent.width
            height: dp(24)
            verticalAlignment: Text.AlignVCenter
        }
    }

    onClicked: {
        if (!active) {
            connectionManager.disconnectFromServer(false)
            mainWindow.currentSystemName = systemName
            LoginDialog.connectToServer(sessionId, address, port, user, password)
        } else {
            sideNavigation.hide()
        }
    }
}
