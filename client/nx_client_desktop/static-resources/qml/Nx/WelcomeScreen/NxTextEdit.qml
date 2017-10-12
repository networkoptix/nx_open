import QtQuick 2.6;
import QtQuick.Controls 1.4;
import QtQuick.Controls.Styles 1.4;
import Nx 1.0;

// TODO: left\right icons\btns implementation

FocusScope
{
    id: control;

    property alias text: textField.text;
    property alias font: textField.font;
    property alias echoMode: textField.echoMode;
    property alias error: textField.error;
    property alias hasFocus: textField.activeFocus;
    property alias textControlEnabled: textField.enabled;
    property Component leftControlDelegate;
    property Component rightControlDelegate;

    property alias leftControl: leftControlLoader.item;
    property alias rightControl: rightControlLoader.item;

    property color backgroundColor: ColorTheme.shadow;
    property color activeColor: ColorTheme.darker(backgroundColor, 1);

    signal accepted();

    onActiveFocusChanged:
    {
        if (activeFocus)
        {
            textField.forceActiveFocus();
            textField.selectAll();
        }
    }

    height: Math.max(leftControlLoader.height, textField.height, rightControlLoader.height);

    Loader
    {
        id: leftControlLoader;

        anchors.verticalCenter: parent.verticalCenter;
        sourceComponent: leftControlDelegate;
        z: 1;
    }

    TextField
    {
        id: textField;

        height: 28;
        anchors.left: (leftControl && leftControl.visible
            ? leftControlLoader.right : parent.left);
        anchors.right: (rightControl && rightControl.visible
            ? rightControlLoader.left : parent.right);

        activeFocusOnPress: true;
        property bool error: false; // TODO: improve logic: autoreset error when text changing

        opacity: (enabled ? 1.0 : 0.3);

        style: nxTextEditStyle;

        onAccepted: control.accepted();
        Keys.enabled: false;
        KeyNavigation.tab: control.KeyNavigation.tab;
        KeyNavigation.backtab: control.KeyNavigation.backtab;

        Component
        {
            id: nxTextEditStyle;

            TextFieldStyle
            {
                background: Rectangle
                {
                    anchors.left: parent.left;
                    anchors.right: parent.right;
                    anchors.leftMargin: (leftControl && leftControl.visible
                        ? -leftControlLoader.width : 0);
                    anchors.rightMargin: (rightControl && rightControl.visible
                        ? -rightControlLoader.width : 0);

                    color:
                    {
                        if (control.readOnly)
                            return ColorTheme.transparent(backgroundColor, 0.2);

                        if (control.activeFocus)
                            return activeColor;

                        return backgroundColor;
                    }

                    radius: 1;
                    border.width: 1;
                    border.color:
                    {
                        if (control.error)
                            return ColorTheme.colors.red_core;
                        if (control.readOnly)
                            return ColorTheme.transparent(ColorTheme.shadow, 0.4);

                        return ColorTheme.darker(color, 1);
                    }

                    Rectangle
                    {
                        id: topInnerShadow;

                        visible: (!control.readOnly && control.activeFocus)
                        anchors.top: parent.top;
                        height: 1;
                        x: 1;
                        width: parent.width - 2 * x;
                        color: ColorTheme.darker(ColorTheme.shadow, 3);
                    }

                    Rectangle
                    {
                        id: rightInnerShadow;

                        visible: (!control.readOnly && control.activeFocus)
                        anchors.right: parent.right;
                        width: 1;
                        y: 1;
                        height: parent.height - 2 * y;
                        color: ColorTheme.darker(ColorTheme.shadow, 3);
                    }
                }

                font.pixelSize: 14;
                font.weight: control.readOnly ? Font.Medium : Font.Normal;
                textColor: ColorTheme.text;

                placeholderTextColor: ColorTheme.midlight;
                renderType: Text.QtRendering;
            }
        }
    }

    Loader
    {
        id: rightControlLoader;

        anchors.verticalCenter: parent.verticalCenter;
        anchors.right: parent.right;
        z: 1;
        sourceComponent: rightControlDelegate;
    }
}
