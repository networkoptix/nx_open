import QtQuick 2.4

import com.networkoptix.qml 1.0

import "../main.js" as Main
import "../controls"

Rectangle {

    property string systemName
    property string host
    property int port

    width: parent.width
    height: dp(72)
    color: QnTheme.sessionItemBackground
    radius: dp(2)

    QnMaterialSurface {
        onClicked: Main.openDiscoveredSession(host, port, systemName)
    }

    Column {
        anchors.verticalCenter: parent.verticalCenter
        spacing: dp(8)
        x: dp(16)
        width: parent.width - 2 * x

        Text {
            text: systemName
            color: QnTheme.listText
            font.pixelSize: sp(16)
            font.weight: Font.Bold
        }

        Text {
            text: host + ":" + port
            color: QnTheme.listSubText
            font.pixelSize: sp(14)
        }
    }
}
