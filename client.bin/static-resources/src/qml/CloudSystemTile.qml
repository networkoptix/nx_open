import QtQuick 2.5;

BaseTile
{
    id: thisComponenet;

    property string userName;

    areaDelegate: Item
    {
        Text
        {
            id: login;

            anchors.top: parent.top;
            anchors.left: parent.left;
            anchors.right: parent.right;

            anchors.topMargin: 2;
            anchors.leftMargin: 4;

            text: userName;
            color: thisComponenet.textColor;
            font.pixelSize: thisComponenet.fontPixelSize;
        }

        Rectangle
        {
            id: cloudIndicator;

            anchors.left: parent.left;
            anchors.bottom: parent.bottom;

            anchors.bottomMargin: 8;

            width: 24;
            height: width;

            color: "lightblue";
        }
    }

    // TODO setup fonts
    readonly property int fontPixelSize: 12;
    readonly property color textColor: "white";
}
