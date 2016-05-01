import QtQuick 2.6
import Qt.labs.controls 1.0
import Nx 1.0

import com.networkoptix.qml 1.0

Button
{
    id: control

    property color color: ColorTheme.base9
    property bool flat: false

    property real mouseX: mouseTracker.mouseX
    property real mouseY: mouseTracker.mouseY

    implicitWidth: Math.max(background ? background.implicitWidth : 0, contentItem ? contentItem.implicitWidth: 0) + leftPadding + rightPadding
    implicitHeight: Math.max(background ? background.implicitHeight : 0, contentItem ? contentItem.implicitHeight : 0) + topPadding + bottomPadding

    padding: 6
    leftPadding: 8
    rightPadding: 8

    font.pixelSize: 16
    font.weight: Font.DemiBold

    contentItem: Text
    {
        text: control.text
        font: control.font
        leftPadding: 8
        rightPadding: 8
        color: ColorTheme.brand_contrast
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        opacity: control.enabled ? 1.0 : 0.3
    }

    background: Rectangle
    {
        implicitWidth: 64
        implicitHeight: 40
        radius: 2

        width: parent.width - leftPadding - rightPadding
        x: parent.leftPadding
        height: parent.height - topPadding - bottomPadding
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
