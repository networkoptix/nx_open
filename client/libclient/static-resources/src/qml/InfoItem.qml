import QtQuick 2.5;

import "."

MaskedComboBox
{
    id: thisComponent;

    property string iconUrl;
    property string hoveredIconUrl: iconUrl;
    property string disabledIconUrl: iconUrl;

    property color disabledLabelColor: Style.label.color;

    anchors.left: parent.left;
    anchors.right: parent.right;

    isEditableComboBox: true;

    onActiveFocusChanged:
    {
        if (!activeFocus || !isAvailable)
            return;

        isMaskedPrivate = true;
        area.forceActiveFocus();
    }

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
                source: (!enabled ? disabledIconUrl
                    : (hoverArea.containsMouse ? hoveredIconUrl : iconUrl));
            }

            NxLabel
            {
                id: textItem;

                disableable: thisComponent.isAvailable;
                isHovered: hoverArea.containsMouse;
                font: Style.fonts.systemTile.info;
                disabledColor: thisComponent.disabledLabelColor;

                Binding
                {
                    target: textItem;
                    property: "text";
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
