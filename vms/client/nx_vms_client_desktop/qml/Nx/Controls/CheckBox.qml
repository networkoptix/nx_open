// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.11
import QtQuick.Controls 2.0

import Nx 1.0

import nx.client.desktop 1.0

CheckBox
{
    id: control

    property Item middleItem: null //< An optional item between indicator and text.
    property alias middleSpacing: middleContainer.rightPadding //< Between middleItem and text.

    padding: 0
    topPadding: 0
    bottomPadding: 0
    leftPadding: 20
    rightPadding: 0

    font.pixelSize: 13
    font.weight: Font.Normal

    baselineOffset: text.baselineOffset + topPadding

    indicator: CheckBoxImage
    {
        id: checkIndicator

        anchors.baseline: control.baseline
        checkState: control.checkState
        enabled: control.enabled
        hovered: control.hovered
    }

    contentItem: Row
    {
        Row
        {
            id: middleContainer

            height: parent.height
            rightPadding: 2
            visible: children.length > 0
        }

        Text
        {
            id: text

            leftPadding: 2
            rightPadding: 2
            verticalAlignment: Text.AlignVCenter
            elide: Qt.ElideRight
            font: control.font
            text: control.text
            opacity: checkIndicator.opacity
            color: checkIndicator.color
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
