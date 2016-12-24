import QtQuick 2.5;

import "."

MaskedComboBox
{
    id: control;

    property string iconUrl;
    property string hoveredIconUrl: iconUrl;
    property string disabledIconUrl: iconUrl;

    property string hoverExtraIconUrl;
    property color disabledLabelColor: Style.colors.midlight;
    property color standardLabelColor: Style.colors.windowText;
    property color hoveredLabelColor: Style.lighterColor(standardLabelColor, 4);

    anchors.left: parent.left;
    anchors.right: parent.right;

    isEditableComboBox: true;

    activeFocusOnTab: true;

    onVisibleChanged:
    {
        if (!visible)
            control.isMaskedPrivate = false;
    }

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

                disableable: control.isAvailable;
                isHovered: hoverArea.containsMouse;
                font: Style.fonts.systemTile.info;
                standardColor: control.standardLabelColor;
                hoveredColor: control.hoveredLabelColor;
                disabledColor: control.disabledLabelColor;

                Binding
                {
                    target: textItem;
                    property: "text";
                    value: control.displayValue;
                }
            }

            Image
            {
                id: pencilImage;

                visible: hoverArea.containsMouse && isAvailable;
                source: hoverExtraIconUrl;
            }
        }

        MouseArea
        {
            id: hoverArea;

            anchors.fill: parent;
            visible: control.isAvailable;

            hoverEnabled: true;
            acceptedButtons: Qt.NoButton;
        }
    }
}
