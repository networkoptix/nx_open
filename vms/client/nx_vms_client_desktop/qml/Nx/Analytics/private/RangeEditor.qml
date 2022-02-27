// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.14

import Nx 1.0
import Nx.Controls 1.0

import nx.vms.client.desktop.analytics 1.0 as Analytics

RowLayout
{
    id: numberEditor

    property Analytics.Attribute attribute

    property alias selectedValue: d.selectedValue

    readonly property alias selectedText: d.selectedText

    spacing: 4

    NumberInput
    {
        id: fromInput

        placeholderText: selectedValue
            ? Utils.getValue(minimum, /*negative infinity*/ "-\u221E")
            : qsTr("from")

        Layout.fillWidth: true

        minimum: attribute ? attribute.minValue : undefined
        maximum: toInput.value

        mode: attribute && attribute.subtype === "int"
            ? NumberInput.Integer
            : NumberInput.Real

        background: TextFieldBackground {}
    }

    Text
    {
        text: "..."
        font.pixelSize: 14
        color: ColorTheme.colors.light16
    }

    NumberInput
    {
        id: toInput

        placeholderText: selectedValue
            ? Utils.getValue(maximum, /*infinity*/ "\u221E")
            : qsTr("to")

        Layout.fillWidth: true

        minimum: fromInput.value
        maximum: attribute ? attribute.maxValue : undefined

        mode: attribute && attribute.subtype === "int"
            ? NumberInput.Integer
            : NumberInput.Real

        background: TextFieldBackground {}
    }

    QtObject
    {
        id: d

        property var selectedValue: undefined
        property string selectedText

        readonly property var selectedValues: [fromInput.value, toInput.value]

        property bool updating: false

        function fromString(stringValue)
        {
            if (!stringValue)
                return undefined

            const value = Number(stringValue)
            return isFinite(value) ? value : undefined
        }

        onSelectedValuesChanged:
        {
            updating = true
            if (selectedValues[0] === undefined && selectedValues[1] === undefined)
            {
                selectedText = ""
                selectedValue = undefined
            }
            else
            {
                const textFrom = Utils.getValue(selectedValues[0], fromInput.placeholderText)
                const textTo = Utils.getValue(selectedValues[1], toInput.placeholderText)
                const ellipsis = "\u2026"
                selectedText = `${textFrom}${ellipsis}${textTo}`

                const from = Utils.getValue(selectedValues[0],
                    Utils.getValue(attribute && attribute.minValue, "-inf"))

                const to = Utils.getValue(selectedValues[1],
                    Utils.getValue(attribute && attribute.maxValue, "inf"))

                selectedValue = `${from}...${to}`
            }

            updating = false
        }

        onSelectedValueChanged:
        {
            if (updating)
                return

            if (selectedValue === undefined)
            {
                fromInput.clear()
                toInput.clear()
            }
            else
            {
                const regExp = /\s*(.*)\s*\.\.\.\s*(.*)\s*/
                const values = regExp.exec(selectedValue)
                fromInput.setValue(fromString(values[1]))
                toInput.setValue(fromString(values[2]))
            }
        }
    }
}
