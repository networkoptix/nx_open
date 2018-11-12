import QtQuick 2.11
import QtQuick.Controls 2.0
import Nx 1.0

GroupBox
{
    id: control

    leftPadding: 0
    rightPadding: 0
    topPadding: 36
    bottomPadding: 20

    font.pixelSize: 15
    font.weight: Font.Medium

    background: Rectangle
    {
        y: 24
        width: parent.availableWidth
        height: 1
        color: ColorTheme.colors.dark12
    }

    label: Text
    {
        width: control.availableWidth
        text: control.title
        font: control.font
        elide: Text.ElideRight
        color: ColorTheme.colors.light4
    }
}
