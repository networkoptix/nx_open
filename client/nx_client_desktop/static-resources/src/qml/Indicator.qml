import QtQuick 2.5;

import ".";

Rectangle
{
    id: control;

    property alias text: labelText.text;
    property alias font: labelText.font;
    property alias textColor: labelText.color;

    property real maxWidth: 0
    property alias sourceTextWidth: labelText.sourceTextWidth

    color: Style.colors.red_main;

    radius: 2;

    implicitHeight: labelText.height;
    implicitWidth: maxWidth ? maxWidth : labelText.sourceTextWidth

    NxLabel
    {
        id: labelText;

        width: parent.width
        leftPadding: 8;
        rightPadding: leftPadding;

        font: Style.fonts.systemTile.indicator;
        elide: control.width < control.sourceTextWidth ? Text.ElideRight : Text.ElideNone
        color: control.textColor;

        verticalAlignment: Text.AlignVCenter;
    }
}
