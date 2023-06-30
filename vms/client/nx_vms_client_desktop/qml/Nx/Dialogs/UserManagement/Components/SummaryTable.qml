// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import Nx.Core 1.0

Row
{
    spacing: 32

    property alias model: columnRepeater.model

    Repeater
    {
        id: columnRepeater

        model: []

        delegate: Column
        {
            spacing: 4

            Text
            {
                font: Qt.font({pixelSize: 14, weight: Font.Normal})
                color: ColorTheme.colors.light16
                text: modelData.label
            }
            Text
            {
                font: Qt.font({pixelSize: 14, weight: Font.Medium})
                color: ColorTheme.colors.light10
                text: modelData.text
            }
        }
    }
}
