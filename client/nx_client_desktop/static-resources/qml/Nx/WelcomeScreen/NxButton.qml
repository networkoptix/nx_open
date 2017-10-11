import QtQuick 2.6;
import Qt.labs.controls 1.0;
import Nx.WelcomeScreen 1.0;

Button
{
    id: control;

    property bool enableHover: true;

    property bool isHovered: hoverArea.containsMouse;
    property bool isAccentButton: false;
    property color bkgColor: (isAccentButton? Style.colors.brand : Style.colors.button);

    property color hoveredColor: Style.lighterColor(bkgColor);
    property color pressedColor: (isAccentButton
      ? Style.darkerColor(hoveredColor) : control.bkgColor);

    property string iconUrl;
    property string hoveredIconUrl;
    property string pressedIconUrl;

    property bool showBackground: true;

    height: 28;

    Keys.enabled: true;
    Keys.onEnterPressed: { control.clicked(); }
    Keys.onReturnPressed: { control.clicked(); }

    Binding
    {
        target: control;
        property: "opacity";
        value: (enabled ? 1.0 : (isAccentButton ? 0.2 : 0.3))
    }

    Image
    {
        id: iconImage;

        anchors.fill: parent;

        visible: control.iconUrl.length;


        function nonEmptyIcon(target, base)
        {
            return (target.length ? target : base);
        }

        source:
        {
            if (control.pressed)
                return nonEmptyIcon(control.pressedIconUrl, control.iconUrl);

            return (control.isHovered && control.hoveredIconUrl.length
                ? control.hoveredIconUrl
                : control.iconUrl);
        }
    }

    MouseArea
    {
        id: hoverArea;

        anchors.fill: parent;
        acceptedButtons: Qt.NoButton;
        hoverEnabled: control.enableHover;
    }

    background: Item
    {
        visible: control.showBackground;
        Rectangle
        {
            anchors.fill: parent;

            color: (control.isHovered && !control.pressed ? control.hoveredColor
                : (control.pressed ? control.pressedColor : control.bkgColor));

            radius: 2;

            border.color: (control.isAccentButton
                ? Style.lighterColor(Style.colors.brand, 2) // TODO: add L4 colro - now it is only 2
                : Style.darkerColor(Style.colors.brand, 4));
            border.width: (control.activeFocus ? 1 : 0);

            Rectangle
            {
                id: borderTop;

                visible: control.pressed;
                x: 1;
                height:1;
                width: parent.width - 2 * x;
                anchors.top: parent.top;
                anchors.topMargin: (control.activeFocus ? 1 : 0);
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
                anchors.bottomMargin: (control.activeFocus ? 1 : 0);
                color: (control.isAccentButton
                    ? Style.darkerColor(Style.colors.brand, 3)
                    : Style.darkerColor(Style.colors.button, control.isHovered ? 1 : 2));
            }
        }
    }

    label: NxLabel
    {
        anchors.centerIn: parent;

        leftPadding: 16;
        rightPadding: 16;

        text: control.text;
        font: Qt.font({ pixelSize: 13, weight: Font.Medium });
        color: (control.isAccentButton
            ? Style.colors.brandContrast
            : Style.colors.buttonText);
    }
}
