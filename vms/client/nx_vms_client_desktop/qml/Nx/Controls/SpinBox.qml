// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14
import QtQuick.Controls 2.4

import Nx 1.0
import Nx.Core 1.0

import nx.vms.client.core 1.0

import "private"

SpinBox
{
    id: control

    property int pageSize: stepSize * 10

    property string suffix: ""

    readonly property real position: (value - from) / (to - from)

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

    property bool _internalUpdate: false

    onValueChanged:
    {
        if (!_internalUpdate && valueFromText(textInput.text, locale) !== value)
            updateText()
    }

    function updateText()
    {
        textInput.text = textFromValue(value, locale)
    }

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

        Keys.onPressed:
        {
            switch (event.key)
            {
                case Qt.Key_PageDown:
                    control.value -= control.pageSize
                    event.accepted = true
                    control.valueModified()
                    break

                case Qt.Key_PageUp:
                    control.value += control.pageSize
                    event.accepted = true
                    control.valueModified()
                    break

                default:
                    break
            }
        }

        WheelHandler
        {
            onWheel:
            {
                const kSensitivity = 1.0 / 120.0
                control.value += control.stepSize * event.angleDelta.y * kSensitivity
                event.accepted = true
                control.valueModified()
            }
        }

        onTextEdited:
        {
            const newValue = text ? control.valueFromText(text, control.locale) : 0
            if (newValue === control.value)
                return

            _internalUpdate = true
            control.value = newValue
            _internalUpdate = false

            control.valueModified()
        }

        onActiveFocusChanged:
        {
            if (!activeFocus)
                control.updateText()
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
        return Number.fromLocaleString(locale, strippedText(text))
    })

    textFromValue: (function(value, locale)
    {
        return decoratedText(Number(value).toLocaleString(locale, 'f', 0))
    })

    function strippedText(text)
    {
        return (!!suffix && text.endsWith(suffix))
            ? text.substr(0, text.length - suffix.length)
            : text
    }

    function decoratedText(text)
    {
        return text + suffix
    }

    Component.onCompleted:
        updateText()
}
