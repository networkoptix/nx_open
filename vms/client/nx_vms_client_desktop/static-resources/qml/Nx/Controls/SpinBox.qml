import QtQuick 2.10
import QtQuick.Controls 2.4

import Nx 1.0

import nx.vms.client.core 1.0

import "private"

SpinBox
{
    id: control

    implicitWidth: Math.max(contentWidth + leftPadding + rightPadding, 64)
    implicitHeight: Math.max(contentHeight, 28) + topPadding + bottomPadding

    leftPadding: control.mirrored ? 18 : 8
    rightPadding: control.mirrored ? 8 : 18
    topPadding: padding
    bottomPadding: padding
    padding: 0

    font.pixelSize: 14

    background: TextFieldBackground { control: parent }

    locale: NxGlobals.numericInputLocale()

    validator: IntValidator
    {
        top: control.to
        bottom: control.from
        locale: control.locale
    }

    property real contentHeight: textInput.implicitHeight

    property real contentWidth: 4 + Math.max(
        fontMetrics.advanceWidth(textFromValue(from, locale)),
        fontMetrics.advanceWidth(textFromValue(to, locale)))

    onValueChanged:
        textInput.updateText()

    contentItem: TextInput
    {
        id: textInput

        z: 0
        color: enabled ? ColorTheme.text : ColorTheme.transparent(ColorTheme.text, 0.3)
        selectByMouse: true
        selectedTextColor: ColorTheme.brightText
        selectionColor: ColorTheme.highlight
        verticalAlignment: Text.AlignVCenter
        inputMethodHints: control.inputMethodHints
        validator: control.validator
        readOnly: !control.editable
        clip: true

        onTextEdited:
        {
            var newValue = control.valueFromText(text, control.locale)
            control.value = newValue

            if (newValue != control.value)
                updateText()
        }

        onEditingFinished:
            text = textFromValue(control.value, control.locale)

        function updateText()
        {
            if (control.valueFromText(text, control.locale) !== control.value)
                text = textFromValue(control.value, control.locale)
        }
    }

    up.indicator: Rectangle
    {
        implicitWidth: 18
        implicitHeight: parent.height / 2
        x: control.mirrored ? 0 : parent.width - width

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
        x: control.mirrored ? 0 : parent.width - width
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

    FontMetrics
    {
        id: fontMetrics
        font: textInput.font
    }

    valueFromText: (function(text, locale)
    {
        return Number.fromLocaleString(locale, text)
    })

    textFromValue: (function(value, locale)
    {
        return Number(value).toLocaleString(locale, 'f', 0)
    })

    Component.onCompleted:
        textInput.text = control.displayText
}
