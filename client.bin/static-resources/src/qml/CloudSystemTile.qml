import QtQuick 2.5;
import QtQuick.Controls 1.2;

BaseTile
{
    id: thisComponent;

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
            color: thisComponent.textColor;
            font.pixelSize: thisComponent.fontPixelSize;
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

    expandedAreaDelegate: Button
    {
        text: qsTr("Connect");
        onClicked: { thisComponent.connectClicked(); }
    }

    // TODO setup fonts
    readonly property int fontPixelSize: 12;
    readonly property color textColor: "white";
}
