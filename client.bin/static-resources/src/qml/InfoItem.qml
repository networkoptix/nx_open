import QtQuick 2.5;

MaskedComboBox
{
    id: thisComponent;

    property string iconUrl;

    anchors.left: parent.left;
    anchors.right: parent.right;

    areaDelegate: Row
    {
        property alias areaText: textItem.text;
        Rectangle
        {
            // TODO: change to image
            id: imageItem;
            width: 16; height: 16; color: (thisComponent.iconUrl.length ? "green" : "red");
        }

        Text
        {
            id: textItem;

            color: thisComponent.textColor;
            font.pixelSize: thisComponent.fontPixelSize;

            Binding
            {
                property: "text";
                target: textItem;
                value: thisComponent.value;
            }
        }
    }

    // TODO setup fonts
    readonly property int fontPixelSize: 12;
    readonly property color textColor: "white";
}
