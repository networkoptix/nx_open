// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick.Controls
import Nx.Core 1.0

import "private"

TextArea
{
    id: textArea

    signal textEdited()

    font.pixelSize: 14

    color: enabled ? ColorTheme.text : ColorTheme.transparent(ColorTheme.text, 0.3)

    background: TextFieldBackground { control: textArea }

    selectByMouse: true
    selectedTextColor: ColorTheme.brightText
    selectionColor: ColorTheme.highlight

    onTextChanged:
    {
        if (activeFocus)
            textArea.textEdited()
    }
}
