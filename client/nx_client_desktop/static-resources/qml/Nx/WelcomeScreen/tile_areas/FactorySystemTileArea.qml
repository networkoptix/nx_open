import QtQuick 2.6;
import Nx 1.0;

import ".."

Column
{
    property string host;
    property string displayHost;
    property alias indicators: indicatorsRow

    topPadding: 2;

    spacing: 10;

    NxLabel
    {
        id: textItem;

        anchors.left: parent.left;
        anchors.leftMargin: 4;

        font: Style.fonts.systemTile.info;
        text: displayHost;
    }

    Item
    {
        width: parent.width
        height: setupControlsRow.height

        Row
        {
            id: setupControlsRow

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

                font: Style.fonts.systemTile.setupSystemLink;
                color: ColorTheme.colors.light12;
                text: qsTr("Click to setup");
            }
        }

        IndicatorsRow
        {
            id: indicatorsRow

            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            maxWidth: parent.width - (setupControlsRow.x + setupControlsRow.width + 14)
        }
    }
}
