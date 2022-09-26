// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

/** Text item with Active Settings support. */
LabeledItem
{
    id: control

    property bool isActive: false

    /** Reference to a TextField item that is a source of signals. */
    property Item targetTextField: contentItem

    /** Emitted when the value is modified by keyboard typing. */
    signal valueEdited()

    /** Emitted to notify about instant value changes (for example, the input is accepted). */
    signal activeValueEdited()

    ActiveTextBehavior {}

    Binding { target: targetTextField; property: "focus"; value: true }

    Connections
    {
        target: targetTextField
        function onTextEdited() { control.valueEdited() }
        function onEditingFinished() { control.activeValueEdited() }
    }

    function getFocusState()
    {
        return targetTextField.cursorPosition
    }

    function setFocusState(state)
    {
        targetTextField.cursorPosition = state
    }
}
