import QtQuick 2.6;
import QtQuick.Controls 1.4;
import QtQuick.Controls.Styles 1.4;

import "."

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

    property color backgroundColor: Style.colors.shadow;
    property color activeColor: Style.darkerColor(backgroundColor);

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
        anchors.left: (leftControlDelegate ? leftControlLoader.right : parent.left);
        anchors.right: (rightControlLoader ? rightControlLoader.left : parent.right);

        activeFocusOnPress: true;
        property bool error: false; // TODO: improve logic: autoreset error when text changing

        opacity: (enabled ? 1.0 : 0.3);

        style: nxTextEditStyle;

        onAccepted: control.accepted();

        Component
        {
            id: nxTextEditStyle;

            TextFieldStyle
            {
                background: Rectangle
                {
                    anchors.left: parent.left;
                    anchors.right: parent.right;
                    anchors.leftMargin: (leftControlLoader.item ? -leftControlLoader.width : 0);
                    anchors.rightMargin: (rightControlLoader.item ? -rightControlLoader.width : 0);

                    color:
                    {
                        if (control.readOnly)
                            return Style.colorWithAlpha(backgroundColor, 0.2);

                        if (control.activeFocus)
                            return activeColor;

                        return backgroundColor;
                    }

                    radius: 1;
                    border.width: 1;
                    border.color:
                    {
                        if (control.error)
                            return Style.colors.red_main;
                        if (control.readOnly)
                            return Style.colorWithAlpha(Style.colors.shadow, 0.4);

                        return Style.darkerColor(color);
                    }

                    Rectangle
                    {
                        id: topInnerShadow;

                        visible: (!control.readOnly && control.activeFocus)
                        anchors.top: parent.top;
                        height: 1;
                        x: 1;
                        width: parent.width - 2 * x;
                        color: Style.darkerColor(Style.colors.shadow, 3);
                    }

                    Rectangle
                    {
                        id: rightInnerShadow;

                        visible: (!control.readOnly && control.activeFocus)
                        anchors.right: parent.right;
                        width: 1;
                        y: 1;
                        height: parent.height - 2 * y;
                        color: Style.darkerColor(Style.colors.shadow, 3);
                    }
                }

                font: (control.readOnly ? Style.textEdit.fontReadOnly : Style.textEdit.font);
                textColor: Style.textEdit.color;

                placeholderTextColor: Style.colors.midlight;
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
