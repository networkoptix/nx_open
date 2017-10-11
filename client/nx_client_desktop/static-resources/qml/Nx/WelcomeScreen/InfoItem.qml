import QtQuick 2.5;
import Nx.WelcomeScreen 1.0;

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
        id: delegate

        height: Math.max(imageItem.height, textItem.height, pencilImage.height);
        width: parent.width

        Image
        {
            id: imageItem;

            anchors.left: parent.left;
            anchors.verticalCenter: parent.verticalCenter;

            width: 16;
            height: 16;
            source: (!enabled ? disabledIconUrl
                : (hoverArea.containsMouse ? hoveredIconUrl : iconUrl));
        }

        NxLabel
        {
            id: textItem;

            x: imageItem.x + imageItem.width
            width: control.isAvailable ? pencilImageHolder.x - x: parent.width - x
            leftPadding: 4;
            rightPadding: 4;
            anchors.verticalCenter: parent.verticalCenter;

            disableable: control.isAvailable;
            isHovered: hoverArea.containsMouse;
            font: Style.fonts.systemTile.info;
            standardColor: control.standardLabelColor;
            hoveredColor: control.hoveredLabelColor;
            disabledColor: control.disabledLabelColor;
            elide: Text.ElideRight;

            Binding
            {
                target: textItem;
                property: "text";
                value: control.displayValue;
            }
        }

        Item
        {
            id: pencilImageHolder;

            property real xPosition: (imageItem.x + imageItem.width + textItem.visibleTextWidth
                + textItem.leftPadding + textItem.rightPadding);
            x: ((xPosition + width) > parent.width
                ? parent.width - pencilImage.width
                : xPosition + textItem.rightPadding);
            anchors.verticalCenter: parent.verticalCenter;
            width: pencilImage.width;
            height: pencilImage.height;

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
