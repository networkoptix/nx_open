import QtQuick 2.4

import com.networkoptix.qml 1.0

import "QnLoginPage.js" as LoginDialog
import "../main.js" as Main

Item {
    id: sessionItem

    property string sessionId
    property string systemName
    property string address
    property int port
    property string user
    property string password

    property alias active: background.active

    width: parent.width
    height: dp(120)

    QnSessionItemBackground {
        id: background
        anchors.fill: parent
    }

    MouseArea {
        anchors.fill: parent
        onClicked: LoginDialog.connectToServer(sessionId, address, port, user, password)
    }

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
            width: parent.width - dp(16) - editButton.width

            Rectangle {
                id: editButton
                color: QnTheme.listSubText
                width: dp(20)
                height: dp(20)
                anchors {
                    verticalCenter: parent.verticalCenter
                    left: parent.right
                    margins: dp(16)
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: Main.openSavedSession(sessionId, address, port, user, password, systemName)
                }
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
}
