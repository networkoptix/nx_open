// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.14

import Nx.Controls 1.0
import Nx.Core 1.0

import nx.vms.client.core.analytics 1.0 as Analytics

FocusScope
{
    id: numberEditor

    property Analytics.Attribute attribute
    property alias selectedValue: d.selectedValue
    readonly property alias selectedText: d.selectedText
    readonly property bool hasTextFields: true

    implicitWidth: content.implicitWidth
    implicitHeight: content.implicitHeight

    function getFocusState()
    {
        let cursorPosition = (input) => input.isValidated()
            ? input.editorCursorPosition
            : String(input.value).length

        if (fromInput.activeFocus && fromInput.value)
            return { fromInput: cursorPosition(fromInput) }

        if (toInput.activeFocus && toInput.value)
            return { toInput: cursorPosition(toInput) }

        return null
    }

    function setFocusState(state)
    {
        if (state.fromInput)
        {
            fromInput.focus = true
            fromInput.cursorPosition = state.fromInput
        }

        if (state.toInput)
        {
            toInput.focus = true
            toInput.cursorPosition = state.toInput
        }
    }

    RowLayout
    {
        id: content

        anchors.fill: numberEditor

        spacing: 4

        NumberInput
        {
            id: fromInput

            color: ColorTheme.brightText
            placeholderTextColor: ColorTheme.windowText
            placeholderText: selectedValue
                ? CoreUtils.getValue(minimum, /*negative infinity*/ "-\u221E")
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

            color: ColorTheme.brightText
            placeholderTextColor: ColorTheme.windowText
            placeholderText: selectedValue
                ? CoreUtils.getValue(maximum, /*infinity*/ "\u221E")
                : qsTr("to")

            Layout.fillWidth: true

            minimum: fromInput.value
            maximum: attribute ? attribute.maxValue : undefined

            mode: attribute && attribute.subtype === "int"
                ? NumberInput.Integer
                : NumberInput.Real

            background: TextFieldBackground {}
        }
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

        function updateSelectedValue()
        {
            updating = true

            if (selectedValues[0] === undefined && selectedValues[1] === undefined)
            {
                selectedText = ""
                selectedValue = undefined
            }
            else
            {
                const textFrom = CoreUtils.getValue(selectedValues[0], fromInput.placeholderText)
                const textTo = CoreUtils.getValue(selectedValues[1], toInput.placeholderText)
                const ellipsis = "\u2026"
                selectedText = `${textFrom}${ellipsis}${textTo}`

                const from = CoreUtils.getValue(selectedValues[0],
                    CoreUtils.getValue(attribute && attribute.minValue, "-inf"))

                const to = CoreUtils.getValue(selectedValues[1],
                    CoreUtils.getValue(attribute && attribute.maxValue, "inf"))

                selectedValue = `${from}...${to}`
            }

            updating = false
        }

        onSelectedValuesChanged:
        {
            if (updating)
                return

            updateSelectedValue()
        }

        onSelectedValueChanged:
        {
            if (updating)
                return

            if (selectedValue === undefined)
            {
                updating = true
                fromInput.clear()
                toInput.clear()
                updating = false

                updateSelectedValue()
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
