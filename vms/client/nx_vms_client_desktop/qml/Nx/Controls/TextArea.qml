// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick.Controls
import Nx.Core 1.0

import "private"

TextArea
{
    id: textArea

    signal textEdited()

    font.pixelSize: 14

    color: enabled ? ColorTheme.colors.light4 : ColorTheme.transparent(ColorTheme.colors.light4, 0.3)

    background: TextFieldBackground { control: textArea }

    selectByMouse: true
    selectedTextColor: ColorTheme.colors.light1
    selectionColor: ColorTheme.colors.brand_core

    onTextChanged:
    {
        if (activeFocus)
            textArea.textEdited()
    }
}
