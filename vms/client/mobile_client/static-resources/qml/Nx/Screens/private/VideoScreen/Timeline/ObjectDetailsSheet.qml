// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Controls
import Nx.Core
import Nx.Core.Controls
import Nx.Core.Items
import Nx.Items
import Nx.Mobile.Controls
import Nx.Ui

import nx.vms.client.core
import nx.vms.client.mobile
import nx.vms.client.mobile.timeline

BaseAdaptiveSheet
{
    id: sheet

    property var model

    // Presumably loaded low-resolution images from the tile.
    property var fallbackImagePaths: []

    readonly property alias count: swipeView.count
    property alias currentIndex: swipeView.currentIndex
    property int objectsType: ObjectsLoader.ObjectsType.motion

    property var dateFormatter: ((timestampMs) => new Date(timestampMs).toLocaleString())

    property bool keepStateOnClose: false

    signal searchRequested(string text)

    spacing: 16
    bottomPadding: (pageNavigationBar.count > 1) ? 0 : 20

    SwipeView
    {
        id: swipeView

        x: -parent.leftPadding
        width: parent.width + parent.leftPadding + parent.rightPadding
        height: contentChildren.reduce((h, child) => Math.max(h, child.implicitHeight), 0)

        // Prevents locking the swipeView position when dragging or scrolling the sheet.
        interactive: sheet.position >= 0.99 && !sheet.contentDragging

        onCurrentIndexChanged:
            pageNavigationBar.index = currentIndex

        Repeater
        {
            id: repeater

            Column
            {
                id: objectInfoColumn

                readonly property real contentWidth: width - leftPadding - rightPadding

                leftPadding: swipeView.parent.leftPadding
                rightPadding: swipeView.parent.rightPadding

                width: swipeView.width
                spacing: 16

                RemoteImage
                {
                    id: image

                    width: objectInfoColumn.contentWidth
                    height: width * (9.0 / 16.0)

                    requestLine: modelData?.imagePath ?? ""

                    // Presumably loaded low-resolution image from the tile.
                    fallbackImagePath:
                    {
                        const reverseIndex = swipeView.count - objectInfoColumn.SwipeView.index - 1
                        const imagePath = sheet.fallbackImagePaths?.[reverseIndex]
                        return imagePath ? `image://remote/${imagePath}` : ""
                    }

                    fallbackImageIfError: true

                    TapHandler
                    {
                        onTapped:
                        {
                            stateController.state = "Expanded"

                            detailsLoader.setSource(Qt.resolvedUrl("../../DetailsScreen.qml"),
                            {
                                "objectsType": sheet.objectsType,
                                "objectData": Qt.binding(() =>
                                    sheet.model?.[sheet.currentIndex] ?? null),
                                "resource": Qt.binding(() =>
                                    sheet.model?.[sheet.currentIndex]?.resource ?? null),
                                "withShowOnCamera": false,
                                "hasNext": Qt.binding(() => sheet.currentIndex + 1 < sheet.count),
                                "hasPrevious": Qt.binding(() => sheet.currentIndex > 0),
                                "toolBar.visible": true,
                                "rightControl": [menuButton, closeDetailsButton]
                            })
                        }
                    }
                }

                Column
                {
                    id: textBlocks

                    spacing: 4
                    width: objectInfoColumn.contentWidth

                    Text
                    {
                        id: timestamp

                        font.pixelSize: 12
                        color: ColorTheme.colors.light16
                        width: parent.width
                        wrapMode: Text.Wrap
                        visible: !!text

                        text:
                        {
                            if (!modelData)
                                return ""

                            const duration = Duration.toString(modelData.durationMs,
                                Duration.Hours | Duration.Minutes | Duration.Seconds,
                                Duration.Short)

                            const date = sheet.dateFormatter
                                ? sheet.dateFormatter(modelData.startTimeMs)
                                : new Date(modelData.startTimeMs).toLocaleString()

                            return `${date} (${duration})`
                        }
                    }

                    Text
                    {
                        id: caption

                        font.pixelSize: 24
                        font.weight: Font.Medium
                        color: ColorTheme.colors.light4
                        text: modelData?.title ?? ""
                        width: parent.width
                        wrapMode: Text.Wrap
                        elide: Text.ElideRight
                        bottomPadding: 2
                        visible: !!text
                    }

                    Text
                    {
                        id: description

                        font.pixelSize: 16
                        font.weight: Font.Normal
                        color: ColorTheme.colors.light10
                        linkColor: ColorTheme.colors.brand_core
                        text: NxGlobals.toHtmlWithLinks(modelData?.description ?? "")
                        width: parent.width
                        wrapMode: Text.Wrap
                        elide: Text.ElideRight
                        textFormat: Text.StyledText
                        visible: !!text

                        onLinkActivated: (link) =>
                        {
                            Workflow.openDialog(
                                "qrc:/qml/Nx/Web/LinkAboutToOpenDialog.qml", {"link": link});
                        }
                    }
                }

                TagView
                {
                    id: tagView

                    width: objectInfoColumn.contentWidth
                    model: modelData?.tags ?? []
                }

                Rectangle
                {
                    height: 1
                    width: objectInfoColumn.contentWidth
                    color: ColorTheme.colors.dark12
                    visible: !!attributeTable.attributes.length
                }

                AnalyticsAttributeTable
                {
                    id: attributeTable

                    width: objectInfoColumn.contentWidth

                    attributes: modelData?.attributes ?? []
                    visible: !!attributes.length

                    nameFont.pixelSize: 14
                    nameColor: ColorTheme.colors.light10
                    valueFont.pixelSize: 14
                    valueColor: ColorTheme.colors.light7
                    valueFont.weight: Font.Medium
                    colorBoxSize: 16
                    rowSpacing: 4
                    labelFraction: 0.5
                    forceLabelFraction: true
                }

                Rectangle
                {
                    height: 1
                    width: objectInfoColumn.contentWidth
                    color: ColorTheme.colors.dark11
                    visible: actions.implicitHeight > 0
                }

                Row
                {
                    id: actions

                    spacing: 8

                    Button
                    {
                        id: downloadButton

                        text: ""
                        type: Button.LightInterface
                        width: 44
                        height: 44
                        icon.width: 24
                        icon.height: 24
                        visible: enabled

                        action: DownloadMediaAction
                        {
                            resource: modelData?.resource ?? null
                            positionMs: modelData?.startTimeMs ?? 0
                            durationMs: modelData?.durationMs ?? 0
                        }
                    }

                    Button
                    {
                        id: shareButton

                        text: ""
                        type: Button.LightInterface
                        width: 44
                        height: 44
                        icon.width: 24
                        icon.height: 24
                        visible: enabled

                        action: ShareAction
                        {
                            objectData: modelData
                            analyticsMode:
                                sheet.objectsType === ObjectsLoader.ObjectsType.analytics
                        }
                    }
                }
            }
        }
    }

    PageNavigationBar
    {
        id: pageNavigationBar

        x: swipeView.x
        width: swipeView.width

        count: sheet.model?.length ?? 0
        visible: count > 1

        onIndexChanged:
            swipeView.setCurrentIndex(index)
    }

    Loader
    {
        id: detailsLoader

        parent: sheet.contentItem
        anchors.fill: parent
        opacity: 0
        z: 100

        Behavior on opacity { NumberAnimation { duration: 150 }}

        onOpacityChanged:
        {
            if (opacity == 0.0) //< Empty itself after a fade-out.
                source = ""
        }

        Connections
        {
            target: detailsLoader.item
            ignoreUnknownSignals: true

            function onLeftButtonClicked()
            {
                stateController.state = ""
            }

            function onPreviousClicked()
            {
                if (swipeView.currentIndex > 0)
                    swipeView.setCurrentIndex(swipeView.currentIndex - 1)
            }

            function onNextClicked()
            {
                const nextIndex = swipeView.currentIndex + 1
                if (nextIndex < swipeView.count)
                    swipeView.setCurrentIndex(nextIndex)
            }

            function onSearchRequested(text)
            {
                sheet.searchRequested(text)
            }
        }

        ToolBarButton
        {
            id: closeDetailsButton

            visible: false

            icon.source: "image://skin/24x24/Outline/close.svg?primary=light10"

            onClicked:
                detailsLoader.item.leftButtonClicked()
        }

        ToolBarButton
        {
            id: menuButton

            visible: !!detailsLoader.item?.menu

            icon.source: "image://skin/24x24/Outline/more.svg?primary=light4"

            onClicked:
                detailsLoader.item.menu.popup(menuButton, 0, menuButton.height)
        }

        states: [
            State //< To adjust the toolbar in landscape orientation.
            {
                name: "Landscape"
                when: detailsLoader.status === Loader.Ready && !LayoutController.isPortrait

                PropertyChanges
                {
                    target: detailsLoader.item

                    title: detailsLoader.item?.resource?.name ?? qsTr("Details")
                    leftButtonIcon.source: ""
                    leftButtonEnabled: false
                }

                PropertyChanges
                {
                    closeDetailsButton.visible: true
                }
            }
        ]
    }

    StateGroup
    {
        id: stateController

        states:
        [
            State
            {
                name: "Expanded"

                PropertyChanges
                {
                    sheet.topPadding: 0
                    sheet.bottomPadding: 0
                    sheet.height: sheet.parent.height - sheet.parent.SafeArea.margins.top
                    detailsLoader.opacity: 1
                }
            }
        ]

        transitions:
        [
            Transition
            {
                NumberAnimation
                {
                    property: "height"
                    duration: 200
                    easing.type: Easing.InOutQuad
                }
            }
        ]
    }

    onModelChanged:
    {
        repeater.model = sheet.model ?? []
        swipeView.setCurrentIndex(swipeView.count ? 0 : -1)
    }

    onClosed:
    {
        if (keepStateOnClose)
            return

        stateController.state = ""
        detailsLoader.source = ""
    }
}
