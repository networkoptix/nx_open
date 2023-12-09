// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx
import Nx.Core

import nx.client.desktop
import nx.vms.client.core
import nx.vms.client.desktop

CheckBox
{
    id: control

    property Item middleItem: null //< An optional item between indicator and text.
    property alias middleSpacing: middleContainer.rightPadding //< Between middleItem and text.
    property alias textLeftPadding: controlText.leftPadding
    property alias textFormat: controlText.textFormat
    property alias wrapMode: controlText.wrapMode
    property alias elide: controlText.elide

    property alias colors: checkIndicator.colors
    property alias checkedColors: checkIndicator.checkedColors

    readonly property alias currentColor: checkIndicator.color

    padding: 0
    topPadding: 1
    bottomPadding: 0
    leftPadding: 20
    rightPadding: 0

    font.pixelSize: FontConfig.normal.pixelSize
    font.weight: Font.Normal

    // Allows to baseline-align other text-based items with the check box.
    // Should be used as read-only.
    baselineOffset: topPadding + firstTextLinePositioner.y + fontMetrics.ascent

    focusPolicy: Qt.TabFocus

    indicator: Item
    {
        id: indicatorHolder

        width: checkIndicator.width
        height: Math.max(checkIndicator.height, firstTextLinePositioner.height)
        y: control.topPadding

        CheckBoxImage
        {
            id: checkIndicator

            anchors.verticalCenter: indicatorHolder.verticalCenter

            checkState: control.checkState
            enabled: control.enabled
            pressed: control.pressed
            hovered: control.hovered
        }

        Item
        {
            id: firstTextLinePositioner

            height: fontMetrics.height
            anchors.verticalCenter: indicatorHolder.verticalCenter

            FontMetrics
            {
                id: fontMetrics
                font: control.font
            }
        }
    }

    contentItem: Item
    {
        id: content

        implicitHeight:
        {
            return Math.max(indicatorHolder.height,
                controlText.relevant ? (controlText.y + controlText.implicitHeight) : 0)
        }

        implicitWidth:
        {
            return controlText.relevant
                ? (controlText.x + controlText.implicitWidth)
                : controlText.x
        }

        Row
        {
            id: middleContainer

            readonly property bool relevant: children.length > 0

            rightPadding: 2
            height: indicatorHolder.height
            visible: relevant

            // Allows to baseline-align possible text-based middle items.
            baselineOffset: control.baselineOffset - control.topPadding
        }

        Text
        {
            id: controlText

            readonly property bool relevant: !!text

            x: middleContainer.relevant ? middleContainer.width : 0
            y: firstTextLinePositioner.y
            width: parent.width - x

            leftPadding: 2
            rightPadding: 2
            elide: Qt.ElideRight
            font: control.font
            text: control.text
            opacity: checkIndicator.opacity
            color: control.currentColor
            visible: relevant
        }
    }

    FocusFrame
    {
        anchors.fill: control.text ? contentItem : indicator
        anchors.margins: control.text ? 1 : 0
        visible: control.visualFocus
        color: ColorTheme.highlight
        opacity: 0.5
    }

    Binding
    {
        target: middleItem
        property: "parent"
        value: middleContainer
    }
}
