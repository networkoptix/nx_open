import QtQuick 2.4

import com.networkoptix.qml 1.0

Item {
    id: button

    property color color: QnTheme.buttonBackground
    property color textColor: QnTheme.buttonText

    property alias text: label.text

    signal clicked()

    height: dp(48)
    width: label.width + dp(24)

    Rectangle {
        anchors {
            fill: parent
            leftMargin: dp(4)
            rightMargin: dp(4)
            topMargin: dp(6)
            bottomMargin: dp(6)
        }

        color: button.color
        radius: dp(2)
        border.width: 0

        QnMaterialSurface {
            onClicked: button.clicked()
        }
    }

    Text {
        id: label

        anchors.centerIn: parent
        color: textColor
        font.pixelSize: sp(14)
        font.weight: Font.DemiBold
        font.capitalization: Font.AllUppercase
    }
}
