import QtQuick 2.4

import com.networkoptix.qml 1.0

Item {
    id: checkBox

    property alias text: label.text
    property bool checked: false

    property real spacing: dp(8)

    signal clicked()

    height: dp(56)
    anchors.left: parent.left
    anchors.right: parent.right

    anchors.leftMargin: -leftPadding
    anchors.rightMargin: -rightPadding

    property real leftPadding: dp(16)
    property real rightPadding: dp(16)

    QnMaterialSurface {
        id: backgroundSurface
        onClicked: {
            checked = !checked
            checkBox.clicked()
        }
    }

    Text {
        id: label
        color: QnTheme.windowText
        x: leftPadding
        anchors.verticalCenter: parent.verticalCenter
        font.weight: Font.Normal
        font.pixelSize: sp(16)
    }

    Rectangle {
        id: indicator

        anchors.right: parent.right
        anchors.rightMargin: rightPadding
        anchors.verticalCenter: parent.verticalCenter
        width: dp(18)
        height: dp(18)

        color: checked ? QnTheme.checkBoxBoxOn : QnTheme.transparent(QnTheme.checkBoxBoxOn, 0)
        border.color: checked ? QnTheme.checkBoxBoxOn : QnTheme.checkBoxBoxOff
        border.width: dp(2)
        radius: dp(2)

        Item {
            id: check
            opacity: checked ? 1.0 : 0.0

            Rectangle {
                x: dp(5.25)
                y: dp(7.5)
                width: dp(12)
                height: dp(2)
                color: QnTheme.windowBackground
                radius: dp(1)
                rotation: -45
            }
            Rectangle {
                x: dp(1.5)
                y: dp(10)
                width: dp(6)
                height: dp(2)
                color: QnTheme.windowBackground
                radius: dp(1)
                rotation: 45
            }
        }
    }
}

