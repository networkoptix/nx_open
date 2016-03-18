import QtQuick 2.6;
import QtQuick.Controls 1.4;
import QtQuick.Controls.Styles 1.4;

import "."

// TODO: improve: generate "clicked" by space key press

Button
{
    id: thisComponent;

    property bool isAccentButton: false;

    height: 28;

    activeFocusOnTab: true;
    opacity: (enabled ? 1.0
        : (isAccentButton ? 0.2 : 0.3));

    style: nxButtonStyle;

    Component
    {
        id: nxButtonStyle;

        ButtonStyle
        {
            readonly property color pressDependentBkg: (control.isAccentButton
                ? (control.pressed ? Style.darkerColor(Style.colors.brand) : Style.colors.brand)
                : Style.colors.button);

            readonly property color hoveredBkg: Style.lighterColor(
                (control.isAccentButton ? Style.colors.brand : Style.colors.button));

            background: Rectangle
            {
                color: (control.hovered && !control.pressed
                    ? hoveredBkg : pressDependentBkg);

                radius: 2;

                border.color: (control.isAccentButton
                    ? Style.lighterColor(Style.colors.brand, 2) // TODO: add L4 colro - now it is only 2
                    : Style.darkerColor(Style.colors.brand, 4));
                border.width: (control.focus ? 1 : 0);

                Rectangle
                {
                    id: borderTop;

                    visible: control.pressed;
                    x: 1;
                    height:1;
                    width: parent.width - 2 * x;
                    anchors.top: parent.top;
                    anchors.topMargin: (control.focus ? 1 : 0);
                    color: (control.isAccentButton
                        ? Style.darkerColor(Style.colors.brand, 3)
                        : Style.darkerColor(Style.colors.button, 2));
                }

                Rectangle
                {
                    id: borderBottom;

                    visible: !control.pressed;
                    x: 1;
                    height:1;
                    width: parent.width - 2 * x;
                    anchors.bottom: parent.bottom;
                    anchors.bottomMargin: (control.focus ? 1 : 0);
                    color: (control.isAccentButton
                        ? Style.darkerColor(Style.colors.brand, 3)
                        : Style.darkerColor(Style.colors.button, control.hovered ? 1 : 2));
                }
            }

            label: NxLabel
            {
                leftPadding: 16;
                rightPadding: 16;
                anchors.fill: parent;

                horizontalAlignment: Qt.AlignHCenter;
                verticalAlignment: Qt.AlignVCenter;
                text: control.text;
                font: Qt.font({ pixelSize: 13, weight: Font.Medium });
                color: (control.isAccentButton
                    ? Style.colors.brandContrast
                    : Style.colors.buttonText);
            }
        }
    }
}
