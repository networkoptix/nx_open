// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx
import Nx.Core
import Nx.Controls

import nx.vms.client.desktop

import "metrics.js" as Metrics

Control
{
    id: control

    property alias displayedItemsText: displayedItemsLabel.text
    property alias availableItemsText: availableItemsButton.text
    property alias showPreview: previewButton.checked

    readonly property int iconWidth: 20
    readonly property int iconHeight: 20

    signal commitNewItemsRequested()

    height: displayedItemsText ? Metrics.kCounterBlockHeight : 0
    visible: !!displayedItemsText

    leftPadding: 8
    rightPadding: 8

    topPadding: 12
    bottomPadding: 12

    contentItem: RowLayout
    {
        id: row

        spacing: 4

        Item
        {
            id: counters

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            readonly property bool hasAvailableItems: !!control.availableItemsText

            Text
            {
                id: displayedItemsLabel

                anchors.fill: parent
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
                color: ColorTheme.mid

                visible: !counters.hasAvailableItems
            }

            AbstractButton
            {
                id: availableItemsButton

                height: 24
                y: (counters.height - height) / 2

                leftPadding: 4
                rightPadding: 4

                visible: counters.hasAvailableItems

                background: Rectangle
                {
                    id: availableItemsBackground

                    color: availableItemsButton.hovered
                        ? ColorTheme.colors.dark6
                        : ColorTheme.colors.dark3

                    border.color: availableItemsButton.hovered && !availableItemsButton.pressed
                        ? ColorTheme.colors.light8
                        : ColorTheme.colors.light10

                    radius: 2
                }

                contentItem: Text
                {
                    text: availableItemsButton.text
                    color: availableItemsBackground.border.color
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }

                onClicked:
                    control.commitNewItemsRequested()
            }
        }

        Row
        {
            id: toolbar

            topPadding: 2
            rightPadding: 7 //< Align to the right border of the rightmost column tile.

            TextButton
            {
                id: previewButton
                text: qsTr("Preview")

                height: 24
                leftPadding: 8
                rightPadding: 8

                checkable: true
                checked: true
                icon.source: "image://svg/skin/advanced_search_dialog/eye_open.svg"
                icon.width: control.iconWidth
                icon.height: control.iconHeight

                color: checked ? ColorTheme.colors.light4 : ColorTheme.colors.light16

                background: Rectangle
                {
                    radius: 2

                    color:
                    {
                        if (previewButton.checked)
                        {
                            return previewButton.hovered
                                ? ColorTheme.colors.dark11
                                : ColorTheme.colors.dark10
                        }

                        return previewButton.hovered
                            ? ColorTheme.colors.dark8
                            : ColorTheme.colors.dark7
                    }
                }
            }
        }
    }

    background: Rectangle
    {
        color: ColorTheme.colors.dark3
    }
}
