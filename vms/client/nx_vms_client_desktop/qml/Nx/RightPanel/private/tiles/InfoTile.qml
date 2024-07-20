// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts

import Nx.Core
import Nx.Core.Controls
import Nx.Controls
import Nx.Effects
import Nx.Items

import nx.vms.client.core
import nx.vms.client.desktop

import ".."
import "../.."
import "../../globals.js" as RightPanelGlobals
import "../metrics.js" as Metrics

TileBase
{
    id: tile

    readonly property bool isCloseable: (model && model.isCloseable) || false

    // The following properties are used by tooltip preview in analytics results.
    readonly property alias previewState: preview.previewState
    readonly property alias videoPreviewTimestampMs: preview.videoPreviewTimestampMs
    readonly property alias videoPreviewResource: preview.videoPreviewResource
    readonly property alias previewAspectRatio: preview.previewAspectRatio
    readonly property alias attributeItems: attributeTable.items

    property alias overlayContainer: overlayContainer

    contentItem: SimpleColumn
    {
        id: tileContent

        spacing: 4
        topPadding: 2

        clip: true

        RowLayout
        {
            id: captionLayout

            spacing: 8
            width: tileContent.width

            ColoredImage
            {
                id: icon

                Layout.alignment: Qt.AlignTop

                sourcePath: (tile.controller && tile.controller.showIcons && model
                    && model.decorationPath) || ""

                sourceSize: Qt.size(20, 20)

                visible: !!sourcePath
                    && (!controller
                        || !controller.attributeManager
                        || controller.attributeManager.objectTypeVisible)
                primaryColor: caption.color
            }

            Text
            {
                id: caption

                visible: !controller
                    || !controller.attributeManager
                    || controller.attributeManager.objectTypeVisible
                Layout.fillWidth: true
                Layout.topMargin: 2
                Layout.alignment: Qt.AlignTop

                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight

                color: (model && model.foregroundColor) || tile.foregroundColor
                font { pixelSize: FontConfig.normal.pixelSize; weight: Font.Medium }

                rightPadding: (isCloseable && !timestamp.text.length)
                    ? (closeButton.width - 2)
                    : 0

                text: (model && (model.objectTitle || model.display)) || ""

                onLinkActivated:
                    tile.linkActivated(link)
            }

            Text
            {
                id: timestamp

                Layout.minimumWidth: implicitWidth
                Layout.topMargin: 2
                Layout.alignment: Qt.AlignTop

                padding: 1
                color: tile.secondaryForegroundColor
                visible: !!text && !(tile.isCloseable && tile.hovered)
                font { pixelSize: FontConfig.small.pixelSize; weight: Font.Normal }

                text: (model && model.textTimestamp) || ""
            }
        }

        Preview
        {
            id: preview

            width: tileContent.width

            topPadding: 2
            topInset: 2

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

            videoPreviewResource: (model && model.previewResource) || null

            Item
            {
                id: overlayContainer
                anchors.fill: preview
            }
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

        NameValueTable
        {
            id: attributeTable

            items: (model && model.filteredAttributes) || []
            visible: items.length > 0 && (!tile.controller || tile.controller.showInformation)
            width: tileContent.width

            nameColor: ColorTheme.colors.light10
            valueColor: ColorTheme.colors.light16
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

        Item
        {
            id: buttonBar

            width: tileContent.width
            implicitHeight: buttonBarContent.height

            visible: buttonBarContent.width > 0

            Row
            {
                id: buttonBarContent

                spacing: 8
                topPadding: 4
                anchors.horizontalCenter: buttonBar.horizontalCenter

                Button
                {
                    action: (model && model.commandAction) || null
                    visible: !!action
                }

                Button
                {
                    action: (model && model.additionalAction) || null
                    visible: !!action
                }
            }
        }

        ResourceList
        {
            id: resourceList

            topPadding: 3
            bottomPadding: 4

            width: tileContent.width
            color: tile.foregroundColor
            remainderColor: tile.secondaryForegroundColor
            resourceNames: (model && model.resourceList) || []
            visible: !empty
                && (!controller
                    || !controller.attributeManager
                    || controller.attributeManager.cameraVisible)
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

        icon.source: "image://skin/20x20/Outline/cross_close.svg"

        onClicked:
        {
            if (tile.controller)
                tile.controller.closeRequested(index)
        }
    }
}
