import QtQuick 2.5;

MaskedComboBox
{
    id: thisComponent;

    property string iconUrl;
    property color textColor;
    property font textFont;

    anchors.left: parent.left;
    anchors.right: parent.right;

    areaDelegate: Row
    {
        spacing: 4;

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
            font: thisComponent.textFont;

            Binding
            {
                property: "text";
                target: textItem;
                value: thisComponent.value;
            }
        }
    }
}
