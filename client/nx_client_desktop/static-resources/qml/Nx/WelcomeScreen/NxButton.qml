import QtQuick 2.6;
import Qt.labs.controls 1.0;
import Nx 1.0;
import Nx.WelcomeScreen 1.0;

Button
{
    id: control;

    property bool enableHover: true;

    property bool isHovered: hoverArea.containsMouse;
    property bool isAccentButton: false;
    property color backgroundColor:
        isAccentButton ? ColorTheme.colors.brand_core : ColorTheme.button;

    property color hoveredColor: ColorTheme.lighter(backgroundColor, 1)
    property color pressedColor:
        isAccentButton ? ColorTheme.darker(hoveredColor, 1) : backgroundColor;

    property string iconUrl;
    property string hoveredIconUrl;
    property string pressedIconUrl;

    property bool showBackground: true;
    property bool flat: false;

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
                : (control.pressed ? control.pressedColor : control.backgroundColor));

            radius: 2;

            border.color: control.isAccentButton
                ? ColorTheme.colors.brand_l4
                : ColorTheme.colors.brand_d4;
            border.width: control.activeFocus ? 1 : 0;

            Rectangle
            {
                id: borderTop;

                visible: !control.flat && control.pressed;
                x: 1;
                height:1;
                width: parent.width - 2 * x;
                anchors.top: parent.top;
                anchors.topMargin: (control.activeFocus ? 1 : 0);
                color: control.isAccentButton
                    ? ColorTheme.colors.brand_d3
                    : ColorTheme.darker(ColorTheme.button, 2);
            }

            Rectangle
            {
                id: borderBottom;

                visible: !control.flat && !control.pressed;
                x: 1;
                height:1;
                width: parent.width - 2 * x;
                anchors.bottom: parent.bottom;
                anchors.bottomMargin: (control.activeFocus ? 1 : 0);
                color: control.isAccentButton
                    ? ColorTheme.colors.brand_d3
                    : ColorTheme.darker(ColorTheme.button, control.isHovered ? 1 : 2);
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
        color: control.isAccentButton ? ColorTheme.colors.brand_contrast : ColorTheme.buttonText;
    }
}
