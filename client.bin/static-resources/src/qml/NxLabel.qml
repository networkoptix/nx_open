import QtQuick 2.6

import "."

Text
{
    id: thisComponent;

    property bool isHovered: false;

    property color standardColor: Style.label.color;
    property color hoveredColor: Style.lighterColor(Style.label.color);
    property color disabledColor: standardColor;

    color:
    {
        if (!enabled)
            return disabledColor;

        return (isHovered ? hoveredColor : standardColor);
    }

    height: Style.label.height;
    font: Style.label.font;

    renderType: Text.NativeRendering;
    verticalAlignment: Text.AlignVCenter;
}
