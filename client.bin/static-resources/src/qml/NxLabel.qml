import QtQuick 2.6

import "."

Text
{
    id: thisComponent;

    property color defaultColor: Style.label.color;

    color: defaultColor;
    height: Style.label.height;
    font: Style.label.font;
    renderType: Text.NativeRendering;
}
