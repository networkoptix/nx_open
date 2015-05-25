import QtQuick 2.2
import QtQuick.Controls.Styles 1.2

import com.networkoptix.qml 1.0

ButtonStyle {
    background: Rectangle {
        color: colorTheme.color("button")
        radius: 2
        border.width: 0

        Rectangle {
            /* shadow */
            width: parent.width
            height: parent.height
            y: 1
            z: -1

            border.width: 0
            radius: parent.radius
            color: Qt.darker(parent.color, 1.5)
        }
    }
    label: Text {
        color: colorTheme.color("buttonText")
        text: control.text
    }
}
