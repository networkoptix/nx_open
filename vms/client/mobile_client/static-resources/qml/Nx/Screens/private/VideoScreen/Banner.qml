import QtQuick 2.6

import Nx 1.0

Rectangle
{
    id: control

    property real maxWidth: 200
    property alias verticalPadding: textItem.topPadding
    property alias horizontalPadding: textItem.leftPadding
    property alias text: textItem.text

    implicitWidth: textItem.width
    implicitHeight: textItem.height

    visible: text.length

    radius: 2
    color: ColorTheme.transparent(ColorTheme.base8, 80)

    Text
    {
        id: textItem

        anchors.centerIn: parent

        readonly property FontMetrics fontMetrics:
            FontMetrics{ font: textItem.font }
        readonly property real textSpace:
            fontMetrics.boundingRect(text).width + leftPadding + rightPadding

        font.pixelSize: 14
        font.weight: Font.Normal
        color: ColorTheme.contrast1

        width: Math.min(textSpace, control.maxWidth)
        leftPadding: 16
        rightPadding: leftPadding
        topPadding: 12
        bottomPadding: topPadding

        wrapMode: Text.WordWrap
    }
}
