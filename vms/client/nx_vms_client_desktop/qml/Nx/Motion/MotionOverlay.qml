// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx
import Nx.Core
import Nx.Core.Items
import Nx.Motion

import nx.vms.client.core

Item
{
    id: motionOverlay

    property int channelIndex: 0
    property MediaPlayerMotionProvider motionProvider: null

    readonly property real kMotionGridOpacity: 0.06
    readonly property real kMotionHighlightOpacity: 0.75

    UniformGrid
    {
        id: grid
        anchors.fill: parent
        opacity: kMotionGridOpacity
        color: ColorTheme.colors.light1
        cellCountX: MotionDefs.gridWidth
        cellCountY: MotionDefs.gridHeight
    }

    MaskedUniformGrid
    {
        id: motionHighlight
        anchors.fill: parent
        opacity: kMotionHighlightOpacity
        color: ColorTheme.colors.red_l2
        cellCountX: MotionDefs.gridWidth
        cellCountY: MotionDefs.gridHeight

        maskTextureProvider: MotionMaskItem
        {
            id: motionMaskItem

            Connections
            {
                target: motionProvider

                function onMotionMaskChanged()
                {
                    motionMaskItem.motionMask = motionProvider.motionMask(channelIndex)
                }
            }
        }
    }
}
