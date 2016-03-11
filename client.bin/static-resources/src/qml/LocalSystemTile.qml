import QtQuick 2.5;

BaseTile
{
    id: thisComponenet;

    property string host;
    property string userName;
    property bool isComaptible;

    collapsedAreaDelegate: Grid
    {
        rows: 2;
        columns: 2;

        rowSpacing: 2;
        columnSpacing: 4;

        anchors.top: parent.top;
        anchors.left: parent.left;
        anchors.right: parent.right;
        anchors.bottom: parent.bottom;

        anchors.topMargin: 12;
        anchors.bottomMargin: 16;

        Rectangle
        {
            id: hostImage;
            width: 16; height: 16; color: (thisComponenet.isComaptible ? "green" : "red");
        }

        Text
        {
            id: hostText;

            text: thisComponenet.host;
            color: thisComponenet.textColor;
            font.pixelSize: thisComponenet.fontPixelSize;
        }

        Rectangle
        {
            id: userImage;
            width: 16; height: 16; color: "white";
        }

        Text
        {
            id: userNameText;

            text: thisComponenet.userName;
            color: thisComponenet.textColor;
            font.pixelSize: thisComponenet.fontPixelSize;
        }
    }

    // TODO setup fonts
    readonly property int fontPixelSize: 12;
    readonly property color textColor: "white";
}
