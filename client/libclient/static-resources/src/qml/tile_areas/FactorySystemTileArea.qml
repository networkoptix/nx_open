import QtQuick 2.6;

import ".."

Column
{
    property string host;
    property string displayHost;

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

            font: Style.fonts.systemTile.setupSystemLink;
            color: Style.colors.custom.systemTile.setupSystemLink;
            text: qsTr("Click to setup");
        }
    }
}
