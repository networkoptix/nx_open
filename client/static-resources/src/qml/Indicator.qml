import QtQuick 2.5;

import ".";

Rectangle
{
    id: thisComponent;

    property string text;
    property font font: Style.fonts.systemTile.indicator;
    property color textColor: Style.colors.shadow;

    color: Style.colors.red_main;

    radius: 2;

    width: labelText.width;
    height: labelText.height;

    NxLabel
    {
        id: labelText;

        leftPadding: 8;
        rightPadding: leftPadding;

        text: thisComponent.text;
        font: thisComponent.font;
        color: thisComponent.textColor;

        verticalAlignment: Text.AlignVCenter;
    }
}
