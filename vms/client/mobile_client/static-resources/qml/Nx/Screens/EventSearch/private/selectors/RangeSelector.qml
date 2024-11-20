// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import Nx.Core 1.0
import Nx.Mobile.Controls 1.0
import nx.vms.client.core.analytics 1.0

import "."

/**
 * Represents range attribute selector.
 */
OptionSelector
{
    id: control

    property Attribute attribute

    unselectedValue: d.getTargetValue(d.makeInnerValue(undefined, undefined))
    valueToTextFunc: (val) => d.getTextValue(val)

    onValueChanged:
    {
        const newInnerValue = d.fromTargetValue(value);
        if (!CoreUtils.equalValues(d.innerValue, newInnerValue))
            d.innerValue = newInnerValue
    }

    screenDelegate: Column
    {
        id: content

        spacing: 4

        signal applyRequested()

        function clear()
        {
            fromInput.value = undefined
            toInput.value = undefined
        }

        NumberInput
        {
            id: fromInput

            width: parent.width
            labelText: qsTr("From")
            mode: d.mode
            value: d.innerValue.from
            minimum: control.attribute ? control.attribute.minValue : undefined
            maximum: toInput.value
            onValueChanged: d.updateValue(value, d.innerValue.to)
            onApplyRequested:
            {
                toInput.selectAll()
                toInput.forceActiveFocus()
            }
        }

        NumberInput
        {
            id: toInput

            width: parent.width
            labelText: qsTr("To")
            value: d.innerValue.to
            mode: d.mode
            minimum: fromInput.value
            maximum: control.attribute ? control.attribute.maxValue : undefined
            onValueChanged: d.updateValue(d.innerValue.from, value)
            onApplyRequested: content.applyRequested()
        }

        Component.onCompleted: fromInput.forceActiveFocus()
    }

    NxObject
    {
        id: d

        property var innerValue: makeInnerValue(undefined, undefined)

        onInnerValueChanged:
        {
            const newValue = d.getTargetValue(d.innerValue)
            if (!CoreUtils.equalValues(control.value, newValue))
                control.value = newValue
        }

        readonly property int mode: control.attribute && control.attribute.subtype === "int"
            ? NumberInput.Integer
            : NumberInput.Real

        function updateValue(fromValue, toValue)
        {
            const newValue = makeInnerValue(fromValue, toValue)
            if (!CoreUtils.equalValues(newValue, d.innerValue))
                d.innerValue = newValue
        }

        function makeInnerValue(fromValue, toValue)
        {
            return { from: fromValue, to: toValue }
        }

        function getTargetValue(value)
        {
            const from = (value && value.from) ?? "-inf";
            const to = (value && value.to) ?? "inf";
            return `${from}...${to}`
        }

        function fromTargetValue(targetValue)
        {
            function fromString(stringValue)
            {
                if (!stringValue)
                    return undefined

                const value = Number(stringValue)
                return isFinite(value) ? value : undefined
            }

            const regExp = /\s*(.*)\s*\.\.\.\s*(.*)\s*/
            const values = regExp.exec(targetValue)
            const fromValue = values ? fromString(values[1]) : undefined
            const toValue = values ? fromString(values[2]) : undefined
            return makeInnerValue(fromValue, toValue)
        }

        function getTextValue(value)
        {
            const result =
                (()=>
                {
                    const val = fromTargetValue(value)
                    if (val.from === undefined && val.to === undefined)
                        return control.unselectedTextValue

                    if (val.from === undefined)
                        return `< ${val.to}`

                    if (val.to === undefined)
                        return `> ${val.from}`

                    return `${val.from} - ${val.to}`
                })()

            const unit = (control.attribute && control.attribute.unit) ?? ""
            return unit && !control.isDefaultValue
                ? `${result} ${unit}`
                : result
        }
    }
}
