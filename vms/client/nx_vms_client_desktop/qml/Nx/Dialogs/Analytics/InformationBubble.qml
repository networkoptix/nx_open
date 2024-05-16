// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Core.Controls
import Nx.Controls
import Nx.Effects
import Nx.Items
import Nx.RightPanel

import nx.vms.client.core
import nx.vms.client.desktop

Bubble
{
	id: bubble

    property EventGrid view: null

    readonly property Item target:
        view && view.hoveredItem && view.hoveredItem.item

    contentItem: Item
    {
        id: content

        implicitWidth: 240

        implicitHeight:
        {
            const maximumHeight = (view ? view.height : Number.MAX_VALUE)
                - bubble.topPadding - bubble.bottomPadding

            const contentHeight = footer.height + spacer.height
                + (attributeTable.relevant ? attributeTable.implicitHeight : 0)

            return Math.min(contentHeight, maximumHeight)
        }

        NameValueTable
        {
            id: attributeTable

            width: content.width

            height: relevant
                ? content.height - footer.height - spacer.height
                : 0

            layer.effect: EdgeOpacityGradient { edges: Qt.BottomEdge; gradientWidth: 40 }
            layer.enabled: relevant && (height < implicitHeight)

            property bool relevant: items.length
            visible: relevant

            nameColor: ColorTheme.colors.light13
            valueColor: ColorTheme.colors.light7
            nameFont { pixelSize: FontConfig.small.pixelSize; weight: Font.Normal }
            valueFont { pixelSize: FontConfig.small.pixelSize; weight: Font.Medium }
        }

        Item
        {
            id: spacer

            property bool relevant: attributeTable.relevant && footer.relevant
            visible: relevant

            anchors.top: attributeTable.bottom
            height: relevant ? 17 : 0
            width: content.width

            Rectangle
            {
                width: content.width
                height: 1
                y: 8
                color: ColorTheme.colors.dark12
            }
        }

        Text
        {
            id: footer

            anchors.top: spacer.bottom
            width: content.width
            horizontalAlignment: Text.AlignHCenter
            textFormat: Text.RichText

            property bool relevant: !!text
            visible: relevant

            color: ColorTheme.colors.light13
            font { pixelSize: FontConfig.small.pixelSize; weight: Font.Normal }
            wrapMode: Text.Wrap
        }

        function getFooterText(engineName)
        {
            if (!engineName)
                return ""

            const detectedBy = qsTr("Detected by")
            return `<div style="font-weight: 200;">${detectedBy}</div> ${engineName}`
        }
    }

    NxObject
    {
        id: d

        property bool suppressed: false

        readonly property var parameters:
        {
            if (!target || !target.attributeItems)
                return undefined

            if (!target.getData("analyticsEngineName") && !target.attributeItems.length)
                return undefined

            const targetRect = view.contentItem.mapFromItem(target,
                -view.contentX, -view.contentY, target.width, target.height)

            const enclosingRect = Qt.rect(0, 0, view.width, view.height)
            const kMinIntersection = 64

            footer.text = content.getFooterText(bubble.target.getData("analyticsEngineName"))
            attributeTable.items = bubble.target.attributeItems

            return calculateParameters(
                Qt.Horizontal, targetRect, enclosingRect, kMinIntersection)
        }

        onParametersChanged:
            updateBubble()

        function setSuppressed(value)
        {
            if (suppressed === value)
                return

            suppressed = value
            updateBubble()
        }

        function updateBubble()
        {
            if (!parameters || suppressed)
            {
                bubble.hide(/*immediately*/ suppressed)
                return
            }

            bubble.pointerEdge = parameters.pointerEdge
            bubble.normalizedPointerPos = parameters.normalizedPointerPos
            bubble.x = parameters.x
            bubble.y = parameters.y
            bubble.show()
        }

        MouseSpy.onMouseMove:
            d.setSuppressed(false)

        MouseSpy.onMousePress:
            d.setSuppressed(true)

        Connections
        {
            target: view
            function onContentYChanged() { d.setSuppressed(true) }
        }
    }

    parent: view
    color: ColorTheme.colors.dark13
}
