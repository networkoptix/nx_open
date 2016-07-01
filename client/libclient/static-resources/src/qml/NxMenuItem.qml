import QtQuick 2.6;
import Qt.labs.controls 1.0 as Labs;

import "."

Labs.MenuItem
{
    id: control;

    implicitWidth: label.width;

    label: Text
    {
        id: label;
        leftPadding: (control.leftPadding ? control.leftPadding : 8);
        rightPadding: (control.rightPadding ? control.rightPadding : 8);

        verticalAlignment: Qt.AlignVCenter;
        horizontalAlignment: Qt.AlignLeft;

        height: Style.menu.height;
        text: control.text;
        color: (hoverArea.containsMouse ? Style.menu.colorHovered : Style.menu.color);
        font: Style.menu.font;
    }


    background: Rectangle
    {
        implicitHeight: Style.menu.height;
        color: (hoverArea.containsMouse ?
            Style.menu.backgroundHovered : Style.menu.background);
    }

    MouseArea
    {
        id: hoverArea;

        anchors.fill: parent;
        hoverEnabled: true;
        acceptedButtons: Qt.NoButton;
    }
}
