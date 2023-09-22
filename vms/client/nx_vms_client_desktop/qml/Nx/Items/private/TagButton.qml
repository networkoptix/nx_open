// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import Nx.Controls 1.0

SelectableTextButton
{
    id: tagButton

    signal cleared(string tag)

    selectable: false
    hoverEnabled: false
    visible: !!text

    onTextChanged:
    {
        setState(text
            ? SelectableTextButton.State.Unselected
            : SelectableTextButton.State.Deactivated)
    }

    onStateChanged:
    {
        if (state != SelectableTextButton.State.Deactivated)
            return

        const tag = text
        text = ""
        cleared(tag)
    }
}
