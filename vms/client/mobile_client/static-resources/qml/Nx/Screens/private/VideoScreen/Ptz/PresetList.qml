// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Core
import Nx.Mobile.Controls

Control
{
    id: control

    property alias model: presets.model
    property int currentIndex: -1

    signal selected(int index)

    spacing: 8

    contentItem: ColumnLayout
    {
        spacing: control.spacing

        Repeater
        {
            id: presets

            delegate: Button
            {
                Layout.fillWidth: true

                text: model.display
                type: Button.LightInterface
                checked: index === currentIndex

                onClicked: control.selected(index)
            }
        }

        Item { Layout.fillHeight: true }
    }
}
