import QtQuick 2.2
import QtQuick.Controls.Styles 1.2

import com.networkoptix.qml 1.0

ButtonStyle {
    id: style

    SystemPalette {
        id: palette
        colorGroup: SystemPalette.Active
    }

    property color color: palette.button
    property color textColor: palette.buttonText

    background: Item {
        implicitHeight: dp(48)
        implicitWidth: dp(120)

        Rectangle {
            anchors {
                fill: parent
                leftMargin: dp(3)
                rightMargin: dp(3)
                topMargin: dp(6)
                bottomMargin: dp(6)
            }

            color: style.color
            radius: dp(2)
            border.width: 0
        }
    }

    label: Item {
        Text {
            anchors.centerIn: parent
            color: textColor
            text: control.text
            font.pixelSize: sp(15)
            font.weight: Font.Bold
        }
    }
}
