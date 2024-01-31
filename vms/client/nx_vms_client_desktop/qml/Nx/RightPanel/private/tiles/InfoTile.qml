// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts

import Nx
import Nx.Core
import Nx.Controls
import Nx.Effects
import Nx.Items

import nx.vms.client.core
import nx.vms.client.desktop

import ".."
import "../.."
import "../../globals.js" as RightPanelGlobals
import "metrics.js" as Metrics

TileBase
{
    id: tile

    readonly property bool isCloseable: (model && model.isCloseable) || false

    // The following properties are used by tooltip preview in analytics results.
    readonly property alias previewState: preview.previewState
    readonly property alias videoPreviewTimestampMs: preview.videoPreviewTimestampMs
    readonly property alias videoPreviewResourceId: preview.videoPreviewResourceId
    readonly property alias previewAspectRatio: preview.previewAspectRatio
    readonly property alias attributeItems: attributeTable.items

    property alias overlayContainer: overlayContainer

    contentItem: SimpleColumn
    {
        id: tileContent

        spacing: 6
        topPadding: 2

        clip: true

        RowLayout
        {
            id: captionLayout

            spacing: 0
            width: tileContent.width

            Item
            {
                implicitWidth: icon.width
                Layout.fillHeight: true
                Layout.rightMargin: 4

                visible: !!icon.decorationPath

                IconImage
                {
                    id: icon

                    readonly property string decorationPath:
                        (tile.controller && tile.controller.showIcons && model && model.decorationPath)
                            || ""

                    anchors.centerIn: parent
                    sourceSize: Qt.size(20, 20)
                    color: caption.color

                    source:
                    {
                        if (!decorationPath)
                            return ""

                        return decorationPath.endsWith(".svg")
                            ? ("image://svg/skin/" + decorationPath)
                            : ("qrc:/skin/" + decorationPath)
                    }
                }
            }

            Text
            {
                id: caption

                Layout.alignment: Qt.AlignLeft | Qt.AlignTop
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight

                color: (model && model.foregroundColor) || tile.foregroundColor
                font { pixelSize: FontConfig.normal.pixelSize; weight: Font.Medium }

                rightPadding: (isCloseable && !timestamp.text.length)
                    ? closeButton.width
                    : 0

                text: (model && model.display) || ""

                onLinkActivated:
                    tile.linkActivated(link)
            }

            Text
            {
                id: timestamp

                Layout.minimumWidth: implicitWidth
                Layout.alignment: Qt.AlignRight | Qt.AlignTop
                Layout.leftMargin: 8

                topPadding: 2
                color: tile.secondaryForegroundColor
                visible: !!text && !(tile.isCloseable && tile.hovered)
                font { pixelSize: FontConfig.small.pixelSize; weight: Font.Normal }

                text: (model && model.timestamp) || ""
            }
        }

        ResourceList
        {
            id: resourceList

            width: tileContent.width
            color: tile.foregroundColor
            remainderColor: tile.secondaryForegroundColor
            resourceNames: (model && model.resourceList) || []
            visible: count > 0
        }

        Preview
        {
            id: preview

            width: tileContent.width

            previewId: (model && model.previewId) || ""
            previewState: (model && model.previewState) || 0
            previewAspectRatio: (model && model.previewAspectRatio) || 1
            highlightRect: (model && model.previewHighlightRect) || Qt.rect(0, 0, 0, 0)

            visible: (model && model.previewResource
                && (!tile.controller || tile.controller.showThumbnails))
                    || false

            videoPreviewEnabled: tile.controller
                && tile.controller.videoPreviewMode != RightPanelGlobals.VideoPreviewMode.none
                && (tile.controller.videoPreviewMode != RightPanelGlobals.VideoPreviewMode.selection
                    || tile.controller.selectedRow === index)

            videoPreviewForced: tile.controller
                && tile.controller.videoPreviewMode == RightPanelGlobals.VideoPreviewMode.selection
                && tile.controller.selectedRow === index

            videoPreviewTimestampMs: (model && NxGlobals.toDouble(model.previewTimestampMs)) || 0

            videoPreviewResourceId: (model && model.previewResource && model.previewResource.id)
                || NxGlobals.uuid("")

            Item
            {
                id: overlayContainer
                anchors.fill: preview
            }
        }

        Text
        {
            id: description

            readonly property string textSource: (model && model.description) || ""

            width: tileContent.width
            height: Math.min(implicitHeight, Metrics.kMaxDescriptionHeight)
            visible: !!text

            color: tile.foregroundColor
            font { pixelSize: FontConfig.small.pixelSize; weight: Font.Normal }
            textFormat: NxGlobals.mightBeHtml(textSource) ? Text.RichText : Text.PlainText
            wrapMode: Text.Wrap

            text: textSource

            layer.effect: EdgeOpacityGradient { edges: Qt.BottomEdge }
            layer.enabled: description.height < description.implicitHeight

            onLinkActivated:
                tile.linkActivated(link)
        }

        NameValueTable
        {
            id: attributeTable

            items: (model && model.attributes) || []
            visible: items.length > 0 && (!tile.controller || tile.controller.showInformation)
            width: tileContent.width

            nameColor: ColorTheme.colors.light16
            valueColor: ColorTheme.colors.light10
            nameFont { pixelSize: FontConfig.small.pixelSize; weight: Font.Normal }
            valueFont { pixelSize: FontConfig.small.pixelSize; weight: Font.Medium }
        }

        Text
        {
            id: footer

            width: tileContent.width
            visible: !!text

            color: tile.foregroundColor
            font { pixelSize: FontConfig.small.pixelSize; weight: Font.Normal }
            textFormat: Text.RichText
            wrapMode: Text.Wrap

            text: (model && model.additionalText) || ""
        }
    }

    ImageButton
    {
        id: closeButton

        visible: tile.isCloseable && tile.hovered

        anchors.right: parent.right
        anchors.rightMargin: 2
        anchors.top: parent.top
        anchors.topMargin: 6

        icon.source: "image://svg/skin/text_buttons/cross_close_20.svg"
        radius: 2

        onClicked:
        {
            if (tile.controller)
                tile.controller.closeRequested(index)
        }
    }
}
