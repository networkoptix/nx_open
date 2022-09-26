// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQml 2.15

import Nx.Controls 1.0

TextFieldWithWarning
{
    id: control
    property var validationRegex: null

    Connections
    {
        target: textField
        function onEditingFinished() { updateWarning() }
    }

    function updateWarning()
    {
        warningState = errorMessage || validationRegex && !validationRegex.test(text)
    }
}
