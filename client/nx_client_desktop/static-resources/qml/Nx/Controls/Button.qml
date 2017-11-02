import QtQuick 2.6
import Qt.labs.controls 1.0
import Nx 1.0
import nx.client.core 1.0
import nx.client.desktop 1.0

Button
{
    id: control

    property color color: highlighted
        ? ColorTheme.highlight
        : ColorTheme.button
    property color textColor: highlighted
        ? ColorTheme.highlightContrast
        : ColorTheme.buttonText

    property color hoveredColor: ColorTheme.lighter(color, 1)
    property color pressedColor: color
    property color checkedColor: ColorTheme.darker(color, 1)
    property color focusIndicatorColor: ColorTheme.isLight(color)
        ? ColorTheme.transparent(ColorTheme.brightText, 0.7)
        : ColorTheme.transparent(ColorTheme.highlight, 0.5)

    property real disabledOpacity: highlighted ? 0.2 : 0.3

    property string icon
    property string hoveredIcon
    property string pressedIcon

    readonly property alias hovered: mouseTracker.containsMouse

    property bool flat: false //< TODO: #dklychkov Remove after updating Qt version.

    leftPadding:
        text
            ? (icon && text) ? 8 : 16
            : 0
    rightPadding: text ? 16 : 0
    topPadding: 5
    bottomPadding: 5

    implicitHeight: Math.max(label.implicitHeight + topPadding + bottomPadding, 28)
    implicitWidth: Math.max(label.implicitWidth + leftPadding + rightPadding, 32)

    Keys.enabled: true
    Keys.onEnterPressed: control.clicked()
    Keys.onReturnPressed: control.clicked()

    font.pixelSize: 13
    font.weight: Font.Medium

    background: Rectangle
    {
        color:
        {
            if (control.pressed)
                return control.pressedColor

            if (control.hovered)
                return control.hoveredColor

            if (control.checked)
                return control.checkedColor

            return control.flat ? ColorTheme.transparent(control.color, 0) : control.color
        }

        radius: 2

        opacity: enabled ? 1.0 : control.disabledOpacity

        Rectangle
        {
            id: borderTop

            visible: enabled && !control.flat && control.pressed
            x: 1
            height: 1
            width: parent.width - 2
            color: ColorTheme.darker(parent.color, 2)
        }

        Rectangle
        {
            id: borderBottom

            visible: enabled && !control.flat && !control.pressed
            x: 1
            height: 1
            width: parent.width - 2
            anchors.bottom: parent.bottom
            color: ColorTheme.darker(parent.color, 2)
        }
    }

    label: Row
    {
        x: image.visible && textLabel.visible
            ? Math.min(
                (control.width - width + leftPadding) / 2 - leftPadding,
                control.width - control.rightPadding - width)
            : (control.width - width) / 2
        y: (control.height - height) / 2

        opacity: enabled ? 1.0 : control.disabledOpacity

        spacing: (image.visible && textLabel.visible) ? Math.max(0, (32 - image.width) / 2) : 0
        leftPadding: spacing

        Image
        {
            id: image

            anchors.verticalCenter: parent.verticalCenter

            visible: status === Image.Ready

            source:
            {
                if (control.pressed)
                    return control.pressedIcon || control.icon

                if (control.hovered)
                    return control.hoveredIcon || control.icon

                return control.icon
            }
        }

        Text
        {
            id: textLabel

            anchors.verticalCenter: parent.verticalCenter
            text: control.text
            font: control.font
            color: control.textColor
            visible: text
        }
    }

    FocusFrame
    {
        anchors.fill: parent
        anchors.margins: 1
        color: focusIndicatorColor
        visible: control.activeFocus
    }

    MouseTracker
    {
        id: mouseTracker
        item: control
        hoverEventsEnabled: true
    }
}
