import QtQuick 2.5;

import "."

MaskedComboBox
{
    id: thisComponent;

    property string iconUrl;
    property string hoveredIconUrl;

    anchors.left: parent.left;
    anchors.right: parent.right;

    isEditableComboBox: true;

    areaDelegate: Item
    {
        width: row.width;
        height: row.height;

        Row
        {
            id: row;
            spacing: 4;

            Image
            {
                id: imageItem;

                width: 16;
                height: 16;
                source: (hoverArea.containsMouse ? hoveredIconUrl : iconUrl);
            }

            NxLabel
            {
                id: textItem;

                color: (hoverArea.containsMouse
                    ? Style.lighterColor(defaultColor, 2) : defaultColor);
                font: Style.fonts.systemTile.info;

                Binding
                {
                    property: "text";
                    target: textItem;
                    value: thisComponent.value;
                }
            }
        }

        MouseArea
        {
            id: hoverArea;

            anchors.fill: parent;
            visible: thisComponent.isAvailable;

            hoverEnabled: true;
            acceptedButtons: Qt.NoButton;
        }
    }
}
