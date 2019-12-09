import QtQuick 2.5;
import Nx 1.0;

Rectangle
{
    id: control;

    property alias text: labelText.text;
    property alias font: labelText.font;
    property alias textColor: labelText.color;

    property real maxWidth: 0
    property alias visibleTextWidth: labelText.visibleTextWidth

    color: ColorTheme.colors.red_core;

    radius: 2;

    implicitHeight: labelText.height;
    implicitWidth: maxWidth ? maxWidth : labelText.visibleTextWidth

    NxLabel
    {
        id: labelText;

        width: parent.width
        leftPadding: 8;
        rightPadding: leftPadding;

        font.pixelSize: 10;
        font.weight: Font.Medium;
        elide: control.width < control.visibleTextWidth ? Text.ElideRight : Text.ElideNone
        color: control.textColor;

        verticalAlignment: Text.AlignVCenter;
    }
}
