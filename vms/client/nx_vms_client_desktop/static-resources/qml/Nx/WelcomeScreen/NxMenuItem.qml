import QtQuick 2.6
import QtQuick.Controls 2.4
import Nx 1.0

MenuItem
{
    id: control;

    implicitWidth: label.implicitWidth + leftPadding + rightPadding;
    implicitHeight: 24;

    anchors.left: parent ? parent.left : undefined
    anchors.right: parent ? parent.right : undefined

    contentItem: Text
    {
        id: label;

        verticalAlignment: Text.AlignVCenter;
        horizontalAlignment: Text.AlignLeft;

        x: control.leftPadding;
        y: control.topPadding;
        width: control.availableWidth;
        height: control.availableHeight;
        text: control.text;
        color: hoverArea.containsMouse ? ColorTheme.colors.brand_contrast :ColorTheme.text;
        font.pixelSize: 13;
    }


    background: Rectangle
    {
        color: hoverArea.containsMouse ? ColorTheme.highlight : ColorTheme.midlight;
    }

    MouseArea
    {
        id: hoverArea;

        anchors.fill: parent;
        hoverEnabled: true;
        acceptedButtons: Qt.NoButton;
    }
}
