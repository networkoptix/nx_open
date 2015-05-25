import QtQuick 2.2
import QtQuick.Controls 1.2
import QtQuick.Controls.Styles 1.2
import QtGraphicalEffects 1.0

import "../common_functions.js" as CommonFunctions

Button {
    id: button

    property color color: "pink"
    property color pressColor: Qt.darker(color, 1.2)
    property color hoverColor: Qt.lighter(color, 1.2)
    property string icon
    property real size: CommonFunctions.dp(56)

    width: size
    height: size

    style: ButtonStyle {
        background: Item {
            id: background

            Rectangle {
                id: rectangle
                anchors.fill: parent
                width: control.width
                height: control.height
                radius: height / 2
                color: {
                    if (control.pressed)
                        return pressColor
                    else if (control.hovered)
                        return hoverColor
                    else
                        return control.color
                }

                Behavior on color { ColorAnimation { duration: 100 } }
            }
        }
        label: Item {
            Image {
                id: image
                anchors.centerIn: parent
                source: control.icon
            }
        }
    }
}
