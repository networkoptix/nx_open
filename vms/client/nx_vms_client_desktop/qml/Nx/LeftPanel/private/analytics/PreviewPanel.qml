// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.11

import Nx 1.0
import Nx.Core 1.0
import Nx.Controls 1.0
import Nx.Items 1.0

import nx.vms.client.desktop 1.0

import "../../../RightPanel/private"

Rectangle
{
    id: previewPanel

    property var selectedItem: null
    property alias slideAnimationEnabled: panelAnimation.enabled

    property alias nextEnabled: intervalPreviewControls.nextEnabled
    property alias prevEnabled: intervalPreviewControls.prevEnabled

    property int loadingIndicatorTimeoutMs: 5000

    signal prevClicked()
    signal nextClicked()
    signal showOnLayoutClicked()

    signal searchRequested(int attributeRow)

    color: ColorTheme.colors.dark8

    Behavior on x
    {
        id: panelAnimation
        enabled: false

        NumberAnimation
        {
            id: slideAnimation
            duration: 200
            easing.type: Easing.Linear

            onRunningChanged:
            {
                // Disable the animation right away to avoid running it on window resize.
                if (!running)
                    panelAnimation.enabled = false
            }
        }
    }

    Rectangle
    {
        id: playerContainer
        color: ColorTheme.colors.dark7
        border.color: ColorTheme.colors.dark5
        radius: 1

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 12

        height: Math.min(width * (9 / 16) + intervalPreviewControls.height, parent.height / 2)

        IntervalPreview
        {
            id: intervalPreview

            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: parent.height - intervalPreviewControls.height

            color: ColorTheme.colors.dark3

            active: !!previewPanel.selectedItem
            loopDelayMs: 0 //< No need to pause because the user has manual controls.
            speedFactor: 1.0 //< This is a regular player (not tile preview) so use normal speed.

            resourceId: previewPanel.selectedItem
                ? previewPanel.selectedItem.previewResourceId : NxGlobals.uuid("")
            timestampMs: previewPanel.selectedItem
                ? NxGlobals.toDouble(previewPanel.selectedItem.previewTimestampMs) : 0
            aspectRatio: previewPanel.selectedItem
                ? previewPanel.selectedItem.previewAspectRatio : 1.0

            Rectangle
            {
                id: previewUnavailablePlaceholder

                anchors.fill: parent
                color: ColorTheme.colors.dark3
                radius: 1

                NxDotPreloader
                {
                    id: preloader

                    anchors.centerIn: parent
                    color: ColorTheme.colors.dark11

                    opacity: running ? 1.0 : 0.0
                    Behavior on opacity { NumberAnimation { duration: 200 }}
                }

                Text
                {
                    id: previewUnavailableText

                    anchors.centerIn: parent
                    width: Math.min(parent.width, 130)

                    color: ColorTheme.colors.dark11
                    font: Qt.font({pixelSize: 12, weight: Font.Normal})

                    text: qsTr("Preview is not available for the selected object")
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap

                    opacity: preloader.running ? 0.0 : 1.0
                    visible: !preloader.running

                    Behavior on opacity { NumberAnimation { duration: 100 }}

                    Timer
                    {
                        interval: previewPanel.loadingIndicatorTimeoutMs
                        running: preloader.running
                        repeat: false

                        onTriggered:
                            preloader.running = false
                    }
                }

                function updateState()
                {
                    const loadingOrMissing = previewPanel.selectedItem
                        && intervalPreview.previewState !== RightPanel.PreviewState.ready

                    preloader.running = loadingOrMissing
                        && intervalPreview.previewState !== RightPanel.PreviewState.missing

                    visible = loadingOrMissing
                }
            }

            onResourceIdChanged:
                previewUnavailablePlaceholder.updateState()

            onPreviewStateChanged:
                previewUnavailablePlaceholder.updateState()
        }

        IntervalPreviewControls
        {
            id: intervalPreviewControls

            anchors.top: intervalPreview.bottom
            anchors.left: parent.left
            anchors.right: parent.right

            preview: intervalPreview
            selectedItem: previewPanel.selectedItem

            onPrevClicked: previewPanel.prevClicked()
            onNextClicked: previewPanel.nextClicked()
        }
    }

    Item
    {
        anchors.left: playerContainer.left
        anchors.top: playerContainer.bottom
        anchors.right: playerContainer.right
        anchors.bottom: buttonBox.top

        Column
        {
            anchors.centerIn: parent

            spacing: 6

            visible: !previewPanel.selectedItem

            Text
            {
                anchors.horizontalCenter: parent.horizontalCenter

                color: ColorTheme.colors.light16
                horizontalAlignment: Text.AlignHCenter

                font.pointSize: 15

                text: qsTr("No Preview")
            }

            Text
            {
                anchors.horizontalCenter: parent.horizontalCenter

                color: ColorTheme.colors.light16
                horizontalAlignment: Text.AlignHCenter

                wrapMode: Text.WordWrap
                width: 150
                font.pointSize: 12

                text: qsTr("Select the object to display the preview")
            }
        }

        Column
        {
            id: dateTimeColumn

            anchors.top: parent.top
            anchors.right: parent.right
            anchors.topMargin: 16

            spacing: 4

            Text
            {
                id: timeText
                color: ColorTheme.colors.light16
                anchors.right: parent.right
                font.pointSize: 17
                text: previewPanel.selectedItem ? previewPanel.selectedItem.timestamp.split(" ", 2).pop() : ""
            }

            Text
            {
                id: dateText
                color: ColorTheme.colors.light16
                anchors.right: parent.right
                font.pointSize: 11
                text:
                {
                    if (!previewPanel.selectedItem)
                        return ""

                    const parts = previewPanel.selectedItem.timestamp.split(" ", 2)
                    return parts.length > 1 ? parts[0] : ""
                }
            }
        }

        Column
        {
            id: dislayColumn

            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: dateTimeColumn.left
            anchors.topMargin: 16

            spacing: 4

            Text
            {
                id: displayText
                color: ColorTheme.colors.light10
                font.pointSize: 17

                width: parent.width
                elide: Text.ElideRight

                text: previewPanel.selectedItem ? previewPanel.selectedItem.display : ""
            }

            ResourceList
            {
                id: resourceList

                width: parent.width

                color: ColorTheme.colors.light10
                remainderColor: ColorTheme.colors.light16

                resourceNames: previewPanel.selectedItem
                    ? previewPanel.selectedItem.resourceList
                    : []
            }
        }

        ScrollView
        {
            id: nameValueScrollView

            anchors.top: dislayColumn.bottom
            anchors.topMargin: 16
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 12
            anchors.left: parent.left
            anchors.leftMargin: -12
            anchors.right: parent.right
            anchors.rightMargin: -12

            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            contentWidth: availableWidth - 8 //< Mind vertical scrollbar width.
            clip: true

            NameValueTable
            {
                id: attributeTable

                items: previewPanel.selectedItem ? previewPanel.selectedItem.attributes : []
                rawItems: previewPanel.selectedItem ? previewPanel.selectedItem.rawAttributes : []
                visible: items.length
                width: nameValueScrollView.contentWidth

                copyable: true

                nameColor: ColorTheme.colors.light16
                valueColor: ColorTheme.colors.light10
                nameFont { pixelSize: 13; weight: Font.Normal }
                valueFont { pixelSize: 13; weight: Font.Normal }

                onSearchRequested: (row) =>
                    previewPanel.searchRequested(row)
            }
        }
    }

    DialogPanel
    {
        id: buttonBox

        color: ColorTheme.colors.dark8
        width: parent.width
        height: 52

        Button
        {
            anchors.right: parent.right
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter

            isAccentButton: true

            enabled: !!previewPanel.selectedItem

            text: qsTr("Show on Layout")

            onClicked:
                previewPanel.showOnLayoutClicked()
        }
    }

    Rectangle
    {
        x: 0
        y: 0
        width: 1
        height: parent.height
        color: ColorTheme.colors.dark9
    }
}
