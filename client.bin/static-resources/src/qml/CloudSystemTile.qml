import QtQuick 2.5;

BaseTile
{
    id: thisComponenet;

    property string userName;

    centralAreaDelegate: Column
    {
        Text
        {
            id: login;

            anchors.left: parent.left;
            anchors.right: parent.right;

            anchors.leftMargin: 4;

            text: userName;
            color: thisComponenet.textColor;
            font.pixelSize: thisComponenet.fontPixelSize;
        }

        Rectangle
        {
            id: cloudIndicator;

            anchors.left: parent.left;

            width: 24;
            height: width;

            color: "lightblue";
        }
    }

    // TODO setup fonts
    readonly property int fontPixelSize: 12;
    readonly property color textColor: "white";
}
