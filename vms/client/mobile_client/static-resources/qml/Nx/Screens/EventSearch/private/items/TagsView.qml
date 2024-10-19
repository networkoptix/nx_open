// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import Nx.Core 1.0

Flow
{
    id: control

    spacing: 6

    readonly property bool hasTags: repeater.count > 0
    property alias model: repeater.model

    Repeater
    {
        id: repeater

        delegate: Rectangle
        {
            height: 24
            width: tagText.width

            color: ColorTheme.colors.light16
            radius: 2

            Text
            {
                id: tagText

                leftPadding: 4
                rightPadding: 4
                width: Math.min(
                    metrics.advanceWidth + leftPadding + rightPadding,
                    scrollableData.width)
                anchors.centerIn: parent
                text: "#" + model.modelData
                color: ColorTheme.colors.dark1
                elide: Text.ElideRight

                TextMetrics
                {
                    id: metrics

                    font: tagText.font
                    text: tagText.text
                }
            }
        }
    }
}
