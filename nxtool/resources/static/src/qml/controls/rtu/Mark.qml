import QtQuick 2.1

Rectangle
{
    id: thisComponent;

    property bool enabled: true;
    property bool selected: false;
    property real borderWidth: 1;

    opacity: (selected && enabled ? 0.2 : (enabled ? 0.0 : 1));
    color: "lightblue";

    border
    {
        width: borderWidth;
        color: (enabled ? "blue" : color);
    }
    radius: 1;
}
