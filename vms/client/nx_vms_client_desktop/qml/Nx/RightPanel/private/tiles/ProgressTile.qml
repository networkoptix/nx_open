// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Controls
import Nx.Effects

import nx.vms.client.core
import nx.vms.client.desktop

import "../metrics.js" as Metrics

TileBase
{
    id: tile

    contentItem: SimpleColumn
    {
        id: tileContent

        spacing: 8
        clip: true

        ProgressBar
        {
            id: progress

            width: tileContent.width

            from: 0
            to: 1

            value: (model && model.progressValue) || 0
            title: (model && model.display) || ""

            font { pixelSize: FontConfig.normal.pixelSize; weight: Font.Medium }
            percentageFont.pixelSize: FontConfig.small.pixelSize
        }

        Text
        {
            id: description

            width: tileContent.width
            height: Math.min(implicitHeight, Metrics.kMaxDescriptionHeight)
            visible: !!text

            color: tile.foregroundColor
            font { pixelSize: FontConfig.small.pixelSize; weight: Font.Normal }
            textFormat: NxGlobals.mightBeHtml(text) ? Text.RichText : Text.PlainText
            wrapMode: Text.Wrap

            text: (model && model.description) || ""

            layer.effect: EdgeOpacityGradient { edges: Qt.BottomEdge }
            layer.enabled: description.height < description.implicitHeight

            onLinkActivated:
                tile.linkActivated(link)
        }
    }
}
