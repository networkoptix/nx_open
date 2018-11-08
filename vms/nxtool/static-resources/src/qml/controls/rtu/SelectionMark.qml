import QtQuick 2.1

Rectangle
{
    id: selectionMark;

    property bool selected: false;
    property real borderWidth: 1;

    opacity: (selected ? 0.2 : 0.0);
    color: "lightblue";

    border
    {
        width: borderWidth;
        color: "blue";
    }
    radius: 1;
}
