import QtQuick 2.5;

MaskedComboBox
{
    id: thisComponent;

    property string text;
    property string iconUrl;

    anchors.left: parent.left;
    anchors.right: parent.right;

    areaDelegate: Row
    {
        Rectangle
        {
            // TODO: change to image
            id: hostImage;
            width: 16; height: 16; color: (thisComponent.iconUrl.length ? "green" : "red");
        }

        Text
        {
            id: hostText;

            text: thisComponent.text;
            color: thisComponent.textColor;
            font.pixelSize: thisComponent.fontPixelSize;
        }
    }

    // TODO setup fonts
    readonly property int fontPixelSize: 12;
    readonly property color textColor: "white";
}
