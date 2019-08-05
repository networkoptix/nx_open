import QtQuick 2.6
import QtQuick.Controls 2.4
import Nx 1.0
import nx.client.desktop 1.0

import "private"

Button
{
    id: control

    property bool isAccentButton: false
    property color backgroundColor:
        isAccentButton ? ColorTheme.colors.brand_core : ColorTheme.button

    property color hoveredColor: ColorTheme.lighter(backgroundColor, 1)
    property color pressedColor:
        isAccentButton ? ColorTheme.darker(hoveredColor, 1) : backgroundColor
    property color outlineColor: ColorTheme.darker(backgroundColor, 2)

    property string iconUrl
    property string hoveredIconUrl
    property string pressedIconUrl

    property bool showBackground: true

    implicitHeight: 28

    font.pixelSize: 13
    font.weight: Font.Medium

    Keys.enabled: true
    Keys.onEnterPressed: control.clicked()
    Keys.onReturnPressed: control.clicked()

    Binding
    {
        target: control
        property: "opacity"
        value: enabled ? 1.0 : (isAccentButton ? 0.2 : 0.3)
    }

    Image
    {
        id: iconImage

        anchors.fill: parent

        visible: control.iconUrl.length

        function nonEmptyIcon(target, base)
        {
            return target.length ? target : base
        }

        source:
        {
            if (control.pressed)
                return nonEmptyIcon(control.pressedIconUrl, control.iconUrl)

            return (control.hovered && control.hoveredIconUrl.length)
                ? control.hoveredIconUrl
                : control.iconUrl
        }
    }

    background: ButtonBackground
    {
        hovered: control.hovered
        pressed: control.pressed
        flat: control.flat
        backgroundColor: control.backgroundColor
        hoveredColor: control.hoveredColor
        pressedColor: control.pressedColor
        outlineColor: control.outlineColor

        FocusFrame
        {
            anchors.fill: parent
            anchors.margins: 1
            visible: control.visualFocus
            color: control.isAccentButton ? ColorTheme.brightText : ColorTheme.highlight
            opacity: 0.5
        }
    }

    contentItem: Text
    {
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter

        leftPadding: 16
        rightPadding: 16

        text: control.text
        font: control.font
        color: control.isAccentButton ? ColorTheme.colors.brand_contrast : ColorTheme.buttonText
    }
}
