import QtQuick 2.5;
import QtQuick.Controls 1.2;

Rectangle
{
    id: thisComponent;

    property string systemName;
    property Component centralAreaDelegate;
    property Component expandedAreaDelegate;
    property bool isExpanded: false;

    function toggle()
    {
        thisComponent.isExpanded = !thisComponent.isExpanded;
    }

    implicitWidth: 280;
    implicitHeight: 96;

    color: palette.button;  // TODO: setup color

    MouseArea
    {
        anchors.fill: parent;

        visible: !thisComponent.isExpanded;
        onClicked: { toggle(); }
    }

    Text
    {
        id: systemNameText;

        anchors.left: parent.left;
        anchors.right: parent.left;
        anchors.top: parent.top;

        anchors.leftMargin: 16;
        anchors.rightMargin: anchors.leftMargin;
        anchors.topMargin: 12;

        text: systemName;
        // TODO: setup color and font
        color: palette.windowText;
        font.pixelSize: 20;
    }

    Button
    {
        id: collapseTileButton;

        width: 40;
        height: 40;

        visible: thisComponent.isExpanded;
        anchors.right: parent.right;
        anchors.top: parent.top;

        text: "x";

        onClicked: { toggle(); }
    }

    Loader
    {
        id: centralAreaLoader;

        anchors.left: parent.left;
        anchors.right: parent.right;
        anchors.top: systemNameText.bottom;
        anchors.bottom: expandedAreaLoader.top;

        anchors.leftMargin: 12;
        anchors.rightMargin: 16;
        sourceComponent: thisComponent.centralAreaDelegate;
    }

    Loader
    {
        id: expandedAreaLoader;

        anchors.left: parent.left;
        anchors.right: parent.right;
        anchors.top: centralAreaLoader.bottom;
        anchors.bottom: parent.bottom;

        anchors.leftMargin: 12;
        anchors.rightMargin: 16;

        sourceComponent: (thisComponenet.isExpanded
            ? thisComponent.expandedAreaDelegate :  undefined);
    }
}
