import QtQuick 2.6
import QtQuick.Controls 2.4
import QtQuick.Controls.impl 2.4
import Nx 1.0
import nx.client.core 1.0

Button
{
    id: control

    property color color: ColorTheme.base9
    property color hoveredColor: Qt.lighter(color)
    property color textColor: ColorTheme.windowText
    property color highlightColor: "#30ffffff"
    property real radius: 2
    property real labelPadding: 24

    property alias mouseX: mouseTracker.mouseX
    property alias mouseY: mouseTracker.mouseY

    implicitWidth: Math.max(
        background ? background.implicitWidth : 0,
        contentItem ? contentItem.implicitWidth : 0) + leftPadding + rightPadding
    implicitHeight: Math.max(
        background ? background.implicitHeight : 0,
        contentItem ? contentItem.implicitHeight : 0) + topPadding + bottomPadding

    padding: 6
    leftPadding: 8
    rightPadding: 8

    font.pixelSize: 16
    font.weight: Font.DemiBold

    icon.color: "transparent"

    onPressAndHold: { d.pressedAndHeld = true }
    onCanceled: { d.pressedAndHeld = false }
    onReleased:
    {
        if (!d.pressedAndHeld)
            return

        d.pressedAndHeld = false
        clicked()
    }

    contentItem: IconLabel
    {
        leftPadding: control.labelPadding
        rightPadding: control.labelPadding
        spacing: 8
        icon: control.icon
        text: control.text
        color: control.textColor
        font: control.font
    }

    background: Rectangle
    {
        implicitWidth: 64
        implicitHeight: 40
        radius: control.radius

        width: parent.availableWidth
        height: parent.availableHeight
        x: parent.leftPadding
        y: parent.topPadding

        color:
        {
            if (control.flat)
                return "transparent"

            if (!control.enabled)
                return control.color

            if (control.highlighted)
                return Qt.lighter(control.color)

            if (control.hovered)
                return control.hoveredColor

            return control.color
        }
        opacity: control.enabled ? 1.0 : 0.3

        MaterialEffect
        {
            anchors.fill: parent
            clip: true
            radius: parent.radius
            mouseArea: control
            rippleSize: 160
            highlightColor: control.highlightColor
        }
    }

    MouseTracker
    {
        id: mouseTracker

        item: control
        hoverEventsEnabled: true
    }

    QtObject
    {
        id: d

        property bool pressedAndHeld: false
    }
}
