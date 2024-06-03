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

    property alias showInformation: informationButton.checked
    property alias showThumbnails: thumbnailsButton.checked

    property bool showInformationButton: false
    property bool showThumbnailsButton: true

    height: displayedItemsText ? Metrics.kCounterBlockHeight : 0
    visible: !!displayedItemsText

    leftPadding: 8
    rightPadding: 8

    contentItem: RowLayout
    {
        id: row

        spacing: 4

        Text
        {
            id: displayedItemsLabel

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            color: ColorTheme.mid
        }

        Row
        {
            id: toolbar

            topPadding: 2
            spacing: 8
            rightPadding: 7 //< Align to the right border of the rightmost column tile.

            TextButton
            {
                id: informationButton

                checkable: true
                checked: true
                icon.source: checked ? "image://skin/20x20/Outline/no_list.svg" : "image://skin/20x20/Outline/event_log.svg"

                color: ColorTheme.colors.light16
                visible: control.showInformationButton
                GlobalToolTip.text: checked ? qsTr("Hide information") : qsTr("Show information")
            }

            TextButton
            {
                id: thumbnailsButton

                checkable: true
                checked: true

                icon.source: checked
                    ? "image://skin/20x20/Outline/image_hide.svg"
                    : "image://skin/20x20/Outline/image.svg"

                color: ColorTheme.colors.light16
                visible: control.showThumbnailsButton
                GlobalToolTip.text: checked ? qsTr("Hide thumbnails") : qsTr("Show thumbnails")
            }
        }
    }

    background: Rectangle
    {
        color: ColorTheme.colors.dark3
    }
}
