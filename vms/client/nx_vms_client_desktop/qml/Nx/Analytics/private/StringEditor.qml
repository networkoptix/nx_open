// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Controls

TextField
{
    id: control

    property var selectedValue: undefined
    readonly property bool hasTextFields: true

    color: ColorTheme.brightText
    placeholderText: qsTr("Min 3 characters")
    placeholderTextColor: ColorTheme.windowText

    onTextChanged:
        selectedValue = text || undefined

    onSelectedValueChanged:
        text = selectedValue || ""

    background: TextFieldBackground {}

    function getFocusState()
    {
        return cursorPosition
    }

    function setFocusState(state)
    {
        cursorPosition = state
    }
}
