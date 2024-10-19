// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import Nx.Mobile.Controls 1.0

/**
 * Represents text attribute selector.
 */
OptionSelector
{
    id: control

    property string labelText: screenTitle
    property var validator: null

    screenDelegate: Item
    {
        id: content

        signal applyRequested()

        implicitHeight: textField.height

        function clear()
        {
            textField.clear()
        }

        TextField
        {
            id: textField

            x: 16
            width: content.width - 2 * x

            labelText: control.labelText
            validator: control.validator

            text: control.value ?? ""

            onTextChanged:
            {
                const trimmed = text.trim()
                const newValue = trimmed === ""
                    ? control.unselectedValue
                    : trimmed
                if (control.value !== newValue)
                    control.value = newValue
            }

            Keys.onReturnPressed: content.applyRequested()

            Component.onCompleted: forceActiveFocus()
        }
    }
}
