import QtQuick 2.6
import Qt.labs.controls 1.0
import Nx 1.0
import nx.client.core 1.0

Button
{
    id: control

    property color color: ColorTheme.base9
    property color hoveredColor: Qt.lighter(color)
    property color textColor: ColorTheme.windowText
    property color highlightColor: "#30ffffff"
    property bool flat: false
    property string icon: ""
    property real radius: 2
    property real labelPadding: 24

    property alias mouseX: mouseTracker.mouseX
    property alias mouseY: mouseTracker.mouseY

    property alias hovered: mouseTracker.containsMouse

    implicitWidth: Math.max(background ? background.implicitWidth : 0,
                            label ? label.implicitWidth: 0) + leftPadding + rightPadding
    implicitHeight: Math.max(background ? background.implicitHeight : 0,
                             label ? label.implicitHeight : 0) + topPadding + bottomPadding

    padding: 6
    leftPadding: 8
    rightPadding: 8

    font.pixelSize: 16
    font.weight: Font.DemiBold

    label: Row
    {
        anchors.centerIn: parent
        leftPadding: control.labelPadding
        rightPadding: control.labelPadding
        spacing: 8

        Image
        {
            source: control.icon
            visible: status == Image.Ready
        }

        Text
        {
            anchors.verticalCenter: parent.verticalCenter
            text: control.text
            font: control.font
            color: control.textColor
            elide: Text.ElideRight
            opacity: control.enabled ? 1.0 : 0.3
        }
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
}
