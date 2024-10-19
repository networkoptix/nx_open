// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import Nx.Core 1.0
import Nx.Mobile.Controls 1.0

import nx.vms.client.core 1.0

Item
{
    id: control

    enum Mode
    {
        Integer,
        Real
    }

    property var value: undefined

    property alias labelText: textField.labelText

    property int mode: NumberInput.Integer
    property var minimum: undefined
    property var maximum: undefined

    signal applyRequested()

    function clear()
    {
        textField.clear()
    }

    implicitHeight: textField.height
    onValueChanged:
    {
        if (d.getValue(textField.text) !== value)
            textField.text = value ?? ""
    }

    TextField
    {
        id: textField

        x: 16
        width: control.width - 2 * 16
        inputMethodHints: Qt.ImhFormattedNumbersOnly

        validator:
        {
            const result = control.mode === NumberInput.Integer
                ? Qt.createQmlObject("import nx.vms.client.core 1.0; IntValidator {}", control)
                : Qt.createQmlObject("import nx.vms.client.core 1.0; DoubleValidator {}", control)

            if (mode === NumberInput.Real)
            {
                result.notation = DoubleValidator.StandardNotation
                result.decimals = 5
            }

            result.locale = NxGlobals.numericInputLocale()
            result.bottom = Qt.binding(() => control.minimum ?? result.lowest)
            result.top = Qt.binding(() => control.maximum ?? result.highest)

            return result
        }

        onActiveFocusChanged:
        {
            if (!activeFocus)
                control.value = d.getValidatedValue(text)
        }

        onAccepted: d.applyValue(text)
        Keys.onEnterPressed: d.applyValue(text)
        Keys.onReturnPressed: d.applyValue(text)
    }

    NxObject
    {
        id: d

        function applyValue(text)
        {
            control.value = d.getValidatedValue(text)
            control.applyRequested()
        }

        function getValue(value)
        {
            if (value === undefined)
                return value

            const trimmed = value.trim()
            if (!trimmed)
                return undefined

            const result = control.mode === NumberInput.Integer
                ? parseInt(trimmed)
                : parseFloat(trimmed)

            return result
        }

        function getValidatedValue(value)
        {
            const result = getValue(value)
            return result === undefined
                ? undefined
                : MathUtils.bound(textField.validator.bottom, result, textField.validator.top)
        }
    }

    onActiveFocusChanged:
    {
        if (activeFocus)
            textField.forceActiveFocus()
    }
}
