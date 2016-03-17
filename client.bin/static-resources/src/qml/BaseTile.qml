import QtQuick 2.5;
import QtQuick.Controls 1.2;

Item
{
    id: thisComponent;

    property string systemName;
    property string host;
    property bool isRecentlyConnected;

    property Component centralAreaDelegate;
    property Component expandedAreaDelegate;
    property bool correctTile: true;
    property bool isExpanded: isExpandedPrivate && correctTile;

    property alias centralArea: centralAreaLoader.item;
    property alias expandedArea: expandedAreaLoader.item;

    property bool isExpandedPrivate: false;

    signal connectClicked();

    function toggle()
    {
        thisComponent.isExpandedPrivate = !thisComponent.isExpandedPrivate;
    }

    implicitWidth: 280;
    implicitHeight: 96;

    z: isExpanded ? 1 : 0

    MouseArea
    {
        anchors.fill: parent;
        visible: !thisComponent.isExpanded;
        onClicked: { toggle(); }
    }

    Rectangle
    {
        MouseArea
        {
            anchors.fill: parent;
            visible: thisComponent.isExpanded;
        }

        x: (thisComponent.isExpanded ? (parent.parent.parent.width - width) / 2 - parent.parent.x : 0);
        y: (thisComponent.isExpanded ? (parent.parent.parent.height - height) / 2 - parent.parent.y : 0);
        width: parent.width;
        height: (thisComponent.isExpanded ? loadersColumn.y + loadersColumn.height : parent.height);

        color: palette.button;  // TODO: setup color

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

        Column
        {
            id: loadersColumn;

            anchors.left: parent.left;
            anchors.right: parent.right;
            anchors.top: systemNameText.bottom;

            anchors.leftMargin: 12;
            anchors.rightMargin: 16;

            Loader
            {
                id: centralAreaLoader;

                anchors.left: parent.left;
                anchors.right: parent.right;

                sourceComponent: thisComponent.centralAreaDelegate;
            }

            Loader
            {
                id: expandedAreaLoader;

                anchors.left: parent.left;
                anchors.right: parent.right;

                visible: thisComponent.isExpanded;
                sourceComponent: thisComponent.expandedAreaDelegate;
            }
        }
    }
}
