// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import Nx.Controls 1.0

TextFieldWithWarning
{
    property var validationRegex: null

    textField.onEditingFinished: updateWarning()

    function updateWarning()
    {
        warningState = validationRegex && !validationRegex.test(text)
    }
}
