// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14

import Nx.Controls 1.0

TextField
{
    id: control

    property var selectedValue: undefined
    readonly property bool hasTextFields: true

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
