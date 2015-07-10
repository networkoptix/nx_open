import QtQuick 2.4

import com.networkoptix.qml 1.0

import "../controls"

Item {
    id: menuItem

    property alias icon: icon.source
    property alias text: label.text

    signal clicked()

    width: content.width + dp(16)
    height: content.height
    clip: true

    QnMaterialSurface {
        onClicked: menuItem.clicked()
        anchors.leftMargin: -dp(8)
        anchors.rightMargin: -dp(8)
        color: "#30000000"
    }

    Row {
        id: content
        height: dp(48)
        x: dp(8)

        spacing: dp(8)

        Image {
            id: icon
            anchors.verticalCenter: parent.verticalCenter
            visible: source ? true : false
        }

        Text {
            id: label
            anchors.verticalCenter: parent.verticalCenter
            color: QnTheme.popupMenuText
            font.weight: Font.Normal
            font.pixelSize: sp(14)
        }
    }
}
