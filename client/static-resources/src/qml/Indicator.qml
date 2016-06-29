import QtQuick 2.5;

import ".";

Rectangle
{
    id: thisComponent;

    property alias text: labelText.text;
    property alias font: labelText.font;
    property alias textColor: labelText.color;

    color: Style.colors.red_main;

    radius: 2;

    width: labelText.width;
    height: labelText.height;

    NxLabel
    {
        id: labelText;

        leftPadding: 8;
        rightPadding: leftPadding;

        font: Style.fonts.systemTile.indicator;
        color: thisComponent.textColor;

        verticalAlignment: Text.AlignVCenter;
    }
}
