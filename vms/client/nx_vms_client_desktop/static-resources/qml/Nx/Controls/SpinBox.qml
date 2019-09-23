import QtQuick 2.10
import QtQuick.Controls 2.0
import Nx 1.0

import "private"

SpinBox
{
    id: control

    implicitWidth: 200
    implicitHeight: 28
    leftPadding: 8
    rightPadding: 18

    font.pixelSize: 14

    background: TextFieldBackground { control: parent }

    onValueChanged:
    {
        if (control.valueFromText(textInput.text, Qt.locale()) !== value)
            textInput.text = control.textFromValue(value, Qt.locale())
    }

    contentItem: TextInput
    {
        id: textInput

        z: 0
        color: enabled ? ColorTheme.text : ColorTheme.transparent(ColorTheme.text, 0.3)
        selectByMouse: true
        selectedTextColor: ColorTheme.brightText
        selectionColor: ColorTheme.highlight

        validator: control.validator

        onTextEdited: control.value = control.valueFromText(text, Qt.locale())
    }

    up.indicator: Rectangle
    {
        implicitWidth: 18
        implicitHeight: parent.height / 2
        x: parent.width - width

        color:
        {
            if (up.pressed)
                return ColorTheme.colors.dark7
            if (up.hovered)
                return ColorTheme.colors.dark8
            return "transparent"
        }

        ArrowIcon
        {
            anchors.centerIn: parent
            color: ColorTheme.text
            opacity: enabled ? 1.0 : 0.3
            rotation: 180
        }
    }

    down.indicator: Rectangle
    {
        implicitWidth: 18
        implicitHeight: parent.height / 2
        x: parent.width - width
        y: parent.height / 2

        color:
        {
            if (down.pressed)
                return ColorTheme.colors.dark7
            if (down.hovered)
                return ColorTheme.colors.dark8
            return "transparent"
        }

        ArrowIcon
        {
            anchors.centerIn: parent
            color: ColorTheme.text
            opacity: enabled ? 1.0 : 0.3
        }
    }

    Component.onCompleted: textInput.text = control.displayText
}
