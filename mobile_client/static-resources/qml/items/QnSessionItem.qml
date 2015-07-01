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
    height: dp(120)

    Column {
        anchors.verticalCenter: parent.verticalCenter
        spacing: dp(8)
        x: dp(16)
        width: parent.width - 2 * x

        Text {
            id: systemNameLabel
            text: systemName
            font.pixelSize: dp(20)
            font.weight: Font.DemiBold
            color: QnTheme.listText
            elide: Text.ElideRight
            width: parent.width - editButton.width

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
            font.pixelSize: dp(16)
            font.weight: Font.DemiBold
            color: QnTheme.listSubText
        }

        Text {
            text: user
            font.pixelSize: dp(16)
            font.weight: Font.DemiBold
            color: QnTheme.listSubText
        }
    }

    onClicked: {
        if (!active) {
            LoginDialog.connectToServer(sessionId, address, port, user, password)
        } else {
            sideNavigation.hide()
        }
    }
}
