import QtQuick 2.4

import com.networkoptix.qml 1.0

QnSideNavigationItem {
    id: buttonItem

    property alias icon: icon.source
    property alias text: label.text

    Image {
        id: icon
        anchors.verticalCenter: parent.verticalCenter
        x: dp(16)
    }

    Text {
        id: label

        anchors.verticalCenter: parent.verticalCenter
        x: dp(72)

        color: QnTheme.listText
        font.pixelSize: sp(14)
        font.weight: Font.DemiBold
    }
}

