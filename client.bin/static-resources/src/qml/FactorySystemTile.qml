import QtQuick 2.6;

import "."

BaseTile
{
    id: thisComponent;

    standardColor: Style.colors.custom.systemTile.factorySystemBkg;
    hoveredColor: Style.colors.custom.systemTile.factorySystemHovered;

    allowExpanding: false;


    centralAreaDelegate: Row
    {
        spacing: 4;

        topPadding: 28; // Check me!

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

    MouseArea
    {
        anchors.fill: parent;
        onClicked: { thisComponent.connectClicked(); }
    }
}
