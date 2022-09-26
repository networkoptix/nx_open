// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import Nx.Controls 1.0

import "private"

ActiveTextItem
{
    id: control

    property alias value: textField.text
    property string defaultValue: ""

    readonly property bool filled: textField.text !== ""

    contentItem: ScrollableTextArea
    {
        id: textField
        text: defaultValue
        textArea.focus: true
    }
}

