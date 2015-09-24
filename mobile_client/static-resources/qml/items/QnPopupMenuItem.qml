import QtQuick 2.4

import com.networkoptix.qml 1.0

import "../controls"

Item {
    id: menuItem

    property alias icon: icon.source
    property alias text: label.text

    signal clicked()

    width: content.width + content.x * 2
    height: content.height
    clip: true

    QnMaterialSurface {
        onClicked: menuItem.clicked()
        color: "#30000000"
    }

    Row {
        id: content
        height: dp(48)
        x: dp(16)

        spacing: dp(8)

        Image {
            id: icon
            anchors.verticalCenter: parent.verticalCenter
            visible: source ? true : false
            scale: iconScale()
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
