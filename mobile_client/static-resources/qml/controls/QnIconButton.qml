import QtQuick 2.2
import QtQuick.Controls 1.2
import QtQuick.Controls.Styles 1.2

import "../common_functions.js" as Common

Button {
    id: button

    property int iconSize: Common.dp(24)
    property string icon

    width: iconSize
    height: iconSize

    style: ButtonStyle {
        background: Item {
        }

        label: Item {
            Image {
                id: image
                source: control.icon
                sourceSize.height: control.iconSize
                sourceSize.width: control.iconSize
                fillMode: Qt.KeepAspectRatio
            }
        }
    }
}
