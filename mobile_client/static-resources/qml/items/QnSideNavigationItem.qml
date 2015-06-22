import QtQuick 2.4

import com.networkoptix.qml 1.0

import "../controls"

Item {
    id: sideNavigationItem

    property alias active: background.active

    signal clicked()

    width: parent.width
    height: dp(56)

    Rectangle {
        id: background

        property bool active

        anchors.fill: parent
        color: active ? QnTheme.sessionItemBackgroundActive : QnTheme.sideNavigationBackground

        Rectangle {
            height: parent.height
            width: dp(3)
            color: QnTheme.sessionItemActiveMark
            visible: active
        }
    }

    QnMaterialSurface {
        onClicked: sideNavigationItem.clicked()
    }
}

