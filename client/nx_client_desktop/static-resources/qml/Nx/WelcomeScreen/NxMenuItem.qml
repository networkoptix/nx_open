import QtQuick 2.6;
import Qt.labs.controls 1.0 as Labs;
import Nx.WelcomeScreen 1.0;

Labs.MenuItem
{
    id: control;

    implicitWidth: label.implicitWidth + leftPadding + rightPadding;
    implicitHeight: Style.menu.height;

    label: Text
    {
        id: label;

        verticalAlignment: Text.AlignVCenter;
        horizontalAlignment: Text.AlignLeft;

        x: control.leftPadding;
        y: control.topPadding;
        width: control.availableWidth;
        height: control.availableHeight;
        text: control.text;
        color: (hoverArea.containsMouse ? Style.menu.colorHovered : Style.menu.color);
        font: Style.menu.font;
    }


    background: Rectangle
    {
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
