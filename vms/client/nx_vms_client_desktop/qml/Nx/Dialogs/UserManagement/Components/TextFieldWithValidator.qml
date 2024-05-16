// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Controls 2.15

import Nx.Core 1.0
import Nx.Controls 1.0

import nx.vms.client.desktop 1.0

TextFieldWithWarning
{
    id: control

    // Avoid the name validator to not confuse with TextField validator property.
    property var validateFunc: (text) => ""
    readonly property bool valid: !warningText

    property var fixupFunc: (text) => text

    warningText: ""

    textField.onTextChanged: warningState = false
    textField.onEditingFinished: validate()

    function validate()
    {
        if (control.fixupFunc)
            text = control.fixupFunc(text)
        warningText = control.validateFunc ? control.validateFunc(text) : ""
        warningState = !!warningText
        return !warningState
    }

    function reset()
    {
        text = ""
        warningText = ""
        warningState = false
    }
}
