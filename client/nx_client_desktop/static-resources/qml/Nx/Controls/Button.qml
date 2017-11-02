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
    property color textHoveredColor: textColor
    property color textPressedColor: textColor
    property color focusIndicatorColor: ColorTheme.isLight(color)
        ? ColorTheme.transparent(ColorTheme.brightText, 0.7)
        : ColorTheme.transparent(ColorTheme.highlight, 0.5)

    property string icon
    property string hoveredIcon
    property string pressedIcon
    property int iconAlignment: Qt.AlignLeft

    property real disabledOpacity: highlighted ? 0.2 : 0.3
    property real iconSpacing: 0
    property real minimumIconWidth: 32

    readonly property alias hovered: mouseTracker.containsMouse

    property bool flat: false //< TODO: #dklychkov Remove after updating Qt version.

    leftPadding:
    {
        if (!text)
            return 0

        if (icon)
            return iconAlignment === Qt.AlignRight ? 12 : 0

        return 16
    }
    rightPadding:
    {
        if (!text)
            return 0

        if (icon)
            return iconAlignment === Qt.AlignRight ? 0 : 12

        return 16
    }
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

    label: Item
    {
        x: control.leftPadding
        y: control.topPadding
        width: parent.availableWidth
        height: parent.availableHeight

        implicitHeight: Math.max(imageArea.implicitHeight, text.implicitHeight)
        implicitWidth: (text.visible ? text.implicitWidth : 0)
            + (imageArea.visible ? imageArea.implicitWidth + control.iconSpacing : 0)

        opacity: enabled ? 1.0 : control.disabledOpacity

        Item
        {
            id: imageArea

            implicitWidth: Math.max(control.minimumIconWidth, image.implicitWidth)
            implicitHeight: image.implicitHeight

            anchors.verticalCenter: parent.verticalCenter
            x: (control.iconAlignment === Qt.AlignRight)
                ? parent.width - width
                : 0

            visible: image.status === Image.Ready

            Image
            {
                id: image

                anchors.centerIn: imageArea

                source:
                {
                    if (control.pressed)
                        return control.pressedIcon || control.icon

                    if (control.hovered)
                        return control.hoveredIcon || control.icon

                    return control.icon
                }

            }
        }

        Text
        {
            id: text

            anchors.verticalCenter: parent.verticalCenter
            x:
            {
                var availableLeft = 0
                var availableRight = parent.width

                if (imageArea.visible)
                {
                    if (control.iconAlignment === Qt.AlignRight)
                        availableRight -= imageArea.width + control.iconSpacing
                    else
                        availableLeft += imageArea.width + control.iconSpacing
                }

                return availableLeft + (availableRight - availableLeft - width) / 2
            }
            width: Math.min(
                parent.width - (imageArea.visible ? imageArea.width + control.iconSpacing : 0),
                implicitWidth)

            text: control.text
            font: control.font
            visible: text

            elide: Text.ElideRight

            color:
            {
                if (control.pressed)
                    return control.textPressedColor

                if (control.hovered)
                    return control.textHoveredColor

                return control.textColor
            }
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
