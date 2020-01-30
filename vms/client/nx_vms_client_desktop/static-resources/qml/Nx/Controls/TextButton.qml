import QtQuick 2.6
import QtQuick.Controls 2.4
import QtQuick.Controls.impl 2.4 as T

import Nx 1.0

AbstractButton
{
    id: button

    property color color: ColorTheme.windowText
    property color hoveredColor: ColorTheme.lighter(color, 2)
    property color pressedColor: color

    font.pixelSize: 13
    hoverEnabled: true
    background: null
    spacing: 2

    contentItem: Row
    {
        id: row

        spacing: button.spacing

        T.IconImage
        {
            id: buttonIcon

            name: button.icon.name
            source: button.icon.source
            visible: !!source
            anchors.verticalCenter: parent.verticalCenter
            color: button.down
                ? button.pressedColor
                : button.hovered ? button.hoveredColor : button.color
        }

        Text
        {
            id: buttonText

            font: button.font
            text: button.text
            visible: !!text
            anchors.verticalCenter: parent.verticalCenter
            color: buttonIcon.color
        }
    }
}
