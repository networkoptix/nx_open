import QtQuick 2.6

import "."

Text
{
    id: control;

    property bool acceptClicks: false;
    property bool autoHoverable: false;
    property bool isHovered: false;
    property bool disableable: true;

    property color standardColor: Style.label.color;
    property color hoveredColor: Style.lighterColor(Style.label.color);
    property color disabledColor: standardColor;
    property var hoveredCursorShape: Qt.ArrowCursor;

    signal clicked();

    opacity: (disableable && !enabled ? 0.3 : 1.0);

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
        target: control;
        property: "isHovered";
        when: control.autoHoverable;
        value: hoverArea.containsMouse;
    }

    MouseArea
    {
        id: hoverArea;

        anchors.fill: parent;
        acceptedButtons: (acceptClicks ? Qt.AllButtons : Qt.NoButton);
        hoverEnabled: control.autoHoverable;

        cursorShape: (control.isHovered
            ? control.hoveredCursorShape
            : Qt.ArrowCursor);

        onClicked: control.clicked();

        visible: (control.acceptClicks || control.autoHoverable) && (control.opacity > 0);
    }
}
