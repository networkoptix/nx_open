import QtQuick 2.11
import QtQuick.Controls 2.0
import QtGraphicalEffects 1.0
import Nx 1.0

GroupBox
{
    id: control

    leftPadding: 16
    rightPadding: 16
    topPadding: 24
    bottomPadding: 16

    font.pixelSize: 13

    background: Item
    {
        anchors.fill: parent
        anchors.topMargin: 8

        Rectangle
        {
            id: borderRect

            anchors.fill: parent
            radius: 2
            color: "transparent"
            border.color: ColorTheme.mid
            visible: false
        }

        Item
        {
            id: borderMask

            anchors.fill: borderRect

            Rectangle
            {
                width: 8
                height: 16
            }
            Rectangle
            {
                x: label.x + label.width + 8
                width: parent.width - x
                height: 16
            }
            Rectangle
            {
                anchors.fill: parent
                anchors.topMargin: 16
            }

            visible: false
        }

        OpacityMask
        {
            anchors.fill: borderRect
            source: borderRect
            maskSource: borderMask
        }
    }

    label: Text
    {
        x: control.leftPadding
        width: Math.min(control.availableWidth, implicitWidth)
        text: control.title
        font: control.font
        elide: Text.ElideRight
        color: ColorTheme.darker(ColorTheme.windowText, 4)
    }
}
