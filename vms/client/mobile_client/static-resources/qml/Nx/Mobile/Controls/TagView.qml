// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Core.Controls

import nx.vms.client.mobile

Flow
{
    id: control

    spacing: 8

    readonly property bool hasTags: repeater.count > 0
    property alias model: repeater.model
    property color color: ColorTheme.colors.dark11
    property int preferredTagHeight: 28

    Repeater
    {
        id: repeater

        delegate: Rectangle
        {
            height: control.preferredTagHeight
            width: tagText.width

            color: control.color
            radius: 4

            Text
            {
                id: tagText

                anchors.centerIn: parent

                leftPadding: 12
                rightPadding: 12

                width:
                {
                    const maxWidth = control.parent.width - leftPadding - rightPadding
                    return Math.min(implicitWidth, maxWidth)
                }
                text: model.modelData
                color: ColorTheme.colors.light10
                elide: Text.ElideRight

                font.pixelSize: 14
                font.weight: 400
            }
        }
    }
}
