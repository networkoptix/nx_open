import QtQuick 2.6

import "."

Text
{
    id: thisComponent;

    property bool acceptClicks: false;
    property bool autoHoverable: false;
    property bool isHovered: false;

    property color standardColor: Style.label.color;
    property color hoveredColor: Style.lighterColor(Style.label.color);
    property color disabledColor: standardColor;
    property var hoveredCursorShape: Qt.ArrowCursor;

    signal clicked();

    color:
    {
        if (!enabled)
            return disabledColor;

        return (isHovered ? hoveredColor : standardColor);
    }

    height: Style.label.height;
    font: Style.label.font;

    renderType: Text.QtRendering;
    verticalAlignment: Text.AlignVCenter;

    Binding
    {
        target: thisComponent;
        property: "isHovered";
        when: thisComponent.autoHoverable;
        value: hoverArea.containsMouse;
    }

    MouseArea
    {
        id: hoverArea;

        anchors.fill: parent;
        acceptedButtons: (acceptClicks ? Qt.AllButtons : Qt.NoButton);
        hoverEnabled: thisComponent.autoHoverable;

        cursorShape: (thisComponent.isHovered
            ? thisComponent.hoveredCursorShape
            : Qt.ArrowCursor);

        onClicked: thisComponent.clicked();
    }
}
