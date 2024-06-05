// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Core

Rectangle
{
    id: control

    property alias displayedItemsText: displayedItemsLabel.text
    property alias availableItemsText: availableItemsButton.text

    signal commitNewItemsRequested()

    implicitWidth:
        Math.max(displayedItemsLabel.implicitWidth, availableItemsButton.implicitWidth, 70)
    implicitHeight:
        Math.max(availableItemsButton.implicitHeight, displayedItemsLabel.implicitHeight)

    readonly property bool hasAvailableItems: !!control.availableItemsText

    color: ColorTheme.colors.dark3

    Text
    {
        id: displayedItemsLabel

        anchors.fill: parent
        leftPadding: 4
        rightPadding: 4
        verticalAlignment: Text.AlignVCenter

        elide: Text.ElideRight
        color: ColorTheme.mid

        visible: !hasAvailableItems
    }

    AbstractButton
    {
        id: availableItemsButton

        height: 32
        anchors.verticalCenter: parent.verticalCenter

        leftPadding: 8
        rightPadding: 8

        visible: hasAvailableItems

        background: Rectangle
        {
            id: availableItemsBackground

            color: availableItemsButton.hovered
                ? ColorTheme.colors.dark6
                : ColorTheme.colors.dark3

            border.color: availableItemsButton.hovered && !availableItemsButton.pressed
                ? ColorTheme.colors.light8
                : ColorTheme.colors.light10

            radius: 4
        }

        contentItem: Text
        {
            text: availableItemsButton.text
            color: availableItemsBackground.border.color
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        onClicked: control.commitNewItemsRequested()
    }
}
