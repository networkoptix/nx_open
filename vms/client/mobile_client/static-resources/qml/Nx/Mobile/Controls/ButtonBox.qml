// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

Row
{
    id: control

    property alias model: repeater.model

    width: parent ? parent.width : implicitWidth
    spacing: repeater.count ? 12 : 0

    signal clicked(string id)

    function buttonById(id)
    {
        for (let i = 0; i !== repeater.count; ++i)
        {
            const button = repeater.itemAt(i)
            if (button.buttonId === id)
                return button
        }

        return null
    }

    Repeater
    {
        id: repeater

        delegate: Button
        {
            readonly property string buttonId: modelData.id

            type: modelData.type
            width: (parent.width + control.spacing) / repeater.count - control.spacing
            text: modelData.text
            onClicked: control.clicked(modelData.id)
        }
    }
}
