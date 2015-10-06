import QtQuick 2.4

import com.networkoptix.qml 1.0

Item {
    id: button

    property color color: QnTheme.buttonBackground
    property color textColor: QnTheme.buttonText
    property bool flat: false
    property bool iconic: icon != "" && text == ""

    property alias text: textLabel.text
    property alias icon: icon.source
    property alias font: textLabel.font

    readonly property alias pressed: materialSurface.pressed

    signal clicked()

    height: dp(48)
    width: label.width + dp(24)

    Rectangle {
        id: background

        anchors {
            fill: parent
            leftMargin: dp(4)
            rightMargin: dp(4)
            topMargin: dp(6)
            bottomMargin: dp(6)
        }

        color: flat ? "transparent" : button.color
        radius: flat ? 0 : dp(2)
        border.width: 0

        QnMaterialSurface {
            id: materialSurface

            circular: button.iconic
            clip: !iconic
            backlight: !iconic
            centered: iconic
            onClicked: button.clicked()
        }
    }

    Row {
        id: label

        anchors.centerIn: parent
        spacing: dp(8)

        Image {
            id: icon
            visible: status == Image.Ready
            anchors.verticalCenter: parent.verticalCenter
            scale: iconScale()
        }

        Text {
            id: textLabel

            visible: text != ""
            anchors.verticalCenter: parent.verticalCenter
            color: textColor
            font.pixelSize: sp(16)
            font.weight: Font.DemiBold
            font.capitalization: flat ? Font.AllUppercase : Font.MixedCase
        }
    }
}
