import QtQuick 2.6
import Qt.labs.controls 1.0
import Nx 1.0

import com.networkoptix.qml 1.0

Button
{
    id: control

    property color color: ColorTheme.base9
    property color textColor: ColorTheme.windowText
    property bool flat: false

    property real mouseX: mouseTracker.mouseX
    property real mouseY: mouseTracker.mouseY

    implicitWidth: Math.max(background ? background.implicitWidth : 0,
                            label ? label.implicitWidth: 0) + leftPadding + rightPadding
    implicitHeight: Math.max(background ? background.implicitHeight : 0,
                             label ? label.implicitHeight : 0) + topPadding + bottomPadding

    padding: 6
    leftPadding: 8
    rightPadding: 8

    font.pixelSize: 16
    font.weight: Font.DemiBold

    label: Text
    {
        anchors.centerIn: parent
        text: control.text
        font: control.font
        leftPadding: 8
        rightPadding: 8
        color: control.textColor
        elide: Text.ElideRight
        opacity: control.enabled ? 1.0 : 0.3
    }

    background: Rectangle
    {
        implicitWidth: 64
        implicitHeight: 40
        radius: 2

        width: parent.availableWidth
        height: parent.availableHeight
        x: parent.leftPadding
        y: parent.topPadding

        color: flat ? "transparent"
                    : control.enabled && control.highlighted ? Qt.lighter(control.color) : control.color
        opacity: control.enabled ? 1.0 : 0.3

        MaterialEffect
        {
            anchors.fill: parent
            clip: true
            radius: parent.radius
            mouseArea: control
            rippleSize: 160
        }
    }

    ItemMouseTracker
    {
        id: mouseTracker
        item: control
    }
}
