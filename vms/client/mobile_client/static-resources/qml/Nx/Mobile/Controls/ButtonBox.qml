// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

Row
{
    id: control

    property alias model: repeater.model

    signal clicked(string id)

    width: parent.width
    spacing: repeater.count ? 12 : 0

    Repeater
    {
        id: repeater

        delegate: Button
        {
            type: modelData.type
            width: (parent.width + control.spacing) / repeater.count - control.spacing
            text: modelData.text
            onClicked: control.clicked(modelData.id)
        }
    }
}
