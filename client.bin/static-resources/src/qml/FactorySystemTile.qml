import QtQuick 2.6;

import "."

BaseTile
{
    id: thisComponent;

    property string host;
    standardColor: Style.colors.custom.systemTile.factorySystemBkg;
    hoveredColor: Style.colors.custom.systemTile.factorySystemHovered;

    isExpandable: false;


    centralAreaDelegate: Column
    {
        topPadding: 2;

        spacing: 10;

        NxLabel
        {
            id: textItem;

            anchors.left: parent.left;
            anchors.leftMargin: 4;

            font: Style.fonts.systemTile.info;
            text: host;
        }

        Row
        {
            spacing: 4;

            Image
            {
                id: imageItem;

                width: 24;
                height: 24;
                source: "qrc:/skin/welcome_page/gears.png";
            }

            NxLabel
            {
                id: setupText;

                anchors.verticalCenter: imageItem.verticalCenter;

                font: Style.fonts.systemTile.setupSystem;
                color: Style.colors.custom.systemTile.setupSystem;
                text: qsTr("Click to setup");
            }
        }
    }

    MouseArea
    {
        anchors.fill: parent;
        onClicked: { thisComponent.connectClicked(); }
    }
}
