import QtQuick 2.5;

Rectangle
{
    id: thisComponent;

    property string systemName;
    property Component areaDelegate;

    implicitWidth: 280;
    implicitHeight: 96;

    color: palette.button;  // TODO: setup color

    Text
    {
        id: systemNameText;

        x: 16;
        y: 12;
        width: parent.width - 2 * x;

        text: systemName;
        // TODO: setup color and font
        color: palette.windowText;
        font.pixelSize: 20;
    }

    Loader
    {
        id: loader;

        anchors.left: parent.left;
        anchors.right: parent.right;
        anchors.top: systemNameText.bottom;
        anchors.bottom: parent.bottom;

        anchors.leftMargin: 12;
        anchors.rightMargin: 16;
        sourceComponent: thisComponent.areaDelegate;
    }
}
