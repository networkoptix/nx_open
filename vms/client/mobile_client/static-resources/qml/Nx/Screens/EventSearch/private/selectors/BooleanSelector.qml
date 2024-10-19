// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import Nx.Mobile.Controls 1.0

import "../editors"

/**
 * Generic boolean value (Yes/No) selector.
 */
OptionSelector
{
    textFieldName: "text"

    valueToTextFunc: (val) =>
    {
        if (val === undefined || val === null)
            return unselectedTextValue

        return val
            ? qsTr("Yes")
            : qsTr("No")
    }

    screenDelegate: RadioGroupEditor
    {
        model: [true, false]
    }
}
