import QtQuick 2.6;
import Qt.labs.controls 1.0;

import "."

Button
{
    id: thisComponent;

    property bool isHovered: hoverArea.containsMouse;
    property bool isAccentButton: false;
    property color bkgColor: (isAccentButton? Style.colors.brand : Style.colors.button);

    property color hoveredColor: Style.lighterColor(bkgColor);
    property color pressedColor: (isAccentButton
      ? Style.darkerColor(hoveredColor) : thisComponent.bkgColor);

    property string iconUrl;

    height: 28;

    opacity: (enabled ? 1.0
        : (isAccentButton ? 0.2 : 0.3));

    Image
    {
        id: iconImage;

        anchors.fill: parent;

        visible: thisComponent.iconUrl.length;
        source: thisComponent.iconUrl;
    }

    MouseArea
    {
        id: hoverArea;

        anchors.fill: parent;
        acceptedButtons: Qt.NoButton;
        hoverEnabled: true;
    }

    background: Item
    {
        Rectangle
        {
            anchors.fill: parent;

            color: (thisComponent.isHovered && !thisComponent.pressed ? thisComponent.hoveredColor
                : (thisComponent.pressed ? thisComponent.pressedColor : thisComponent.bkgColor));

            radius: 2;

            border.color: (thisComponent.isAccentButton
                ? Style.lighterColor(Style.colors.brand, 2) // TODO: add L4 colro - now it is only 2
                : Style.darkerColor(Style.colors.brand, 4));
            border.width: (thisComponent.activeFocus ? 1 : 0);

            Rectangle
            {
                id: borderTop;

                visible: thisComponent.pressed;
                x: 1;
                height:1;
                width: parent.width - 2 * x;
                anchors.top: parent.top;
                anchors.topMargin: (thisComponent.activeFocus ? 1 : 0);
                color: (thisComponent.isAccentButton
                    ? Style.darkerColor(Style.colors.brand, 3)
                    : Style.darkerColor(Style.colors.button, 2));
            }

            Rectangle
            {
                id: borderBottom;

                visible: !thisComponent.pressed;
                x: 1;
                height:1;
                width: parent.width - 2 * x;
                anchors.bottom: parent.bottom;
                anchors.bottomMargin: (thisComponent.activeFocus ? 1 : 0);
                color: (thisComponent.isAccentButton
                    ? Style.darkerColor(Style.colors.brand, 3)
                    : Style.darkerColor(Style.colors.button, thisComponent.isHovered ? 1 : 2));
            }
        }
    }

    label: NxLabel
    {
        anchors.centerIn: parent;

        leftPadding: 16;
        rightPadding: 16;

        text: thisComponent.text;
        font: Qt.font({ pixelSize: 13, weight: Font.Medium });
        color: (thisComponent.isAccentButton
            ? Style.colors.brandContrast
            : Style.colors.buttonText);
    }
}
