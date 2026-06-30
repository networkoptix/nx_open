// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

import Nx.Common
import Nx.Controls
import Nx.Core
import Nx.Core.Controls
import Nx.Core.EventSearch
import Nx.Core.Items
import Nx.Core.Ui
import Nx.Items
import Nx.Mobile
import Nx.Mobile.Controls as MobileControls
import Nx.Models
import Nx.Screens
import Nx.Settings
import Nx.Ui
import nx.vms.client.core
import nx.vms.client.mobile.timeline as Timeline

import "DetailsScreen"

Page
{
    id: eventDetailsScreen

    objectName: "eventDetailsScreen"

    property var uuid
    property int objectsType: Timeline.ObjectsLoader.ObjectsType.analytics
    property QnCameraListModel camerasModel: null
    required property Timeline.AbstractObjectData objectData
    required property Resource resource
    readonly property alias fullscreenLayout: preview.fullscreenLayout
    readonly property Menu menu: menu.available ? menu : null
    property alias hasNext: preview.hasNext
    property alias hasPrevious: preview.hasPrevious
    property alias withShowOnCamera: preview.withShowOnCamera

    property bool showPreviewImage: !LayoutController.isTabletLayout
        && objectsType === Timeline.ObjectsLoader.ObjectsType.analytics

    signal fullscreenButtonClicked()
    signal searchRequested(string text)
    signal nextClicked()
    signal previousClicked()

    title: eventDetailsScreen.resource?.name ?? qsTr("Preview")
    toolBar.visible: false

    clip: false

    Preview
    {
        id: preview

        width: parent.width
        height:
        {
            if (d.portraitOrientation)
                return heightForWidth(width)

            return eventDetailsScreen.height
        }
        fullscreenLayout: !d.portraitOrientation
        activePage: eventDetailsScreen.activePage

        onNext: eventDetailsScreen.nextClicked()
        onPrevious: eventDetailsScreen.previousClicked()
        onShowFullscreen: eventDetailsScreen.fullscreenButtonClicked()
        onShowOnCamera: d.goToCamera()
    }

    GridLayout
    {
        id: detailsLayout

        readonly property real kHorizontalLayoutRatio: 1.5
        property bool horizontal: width / height > kHorizontalLayoutRatio

        anchors.top: preview.bottom
        anchors.right: parent.right
        anchors.left: parent.left
        anchors.bottom: parent.bottom

        rowSpacing: 0
        columnSpacing: 0

        flow: horizontal ? GridLayout.LeftToRight : GridLayout.TopToBottom
        clip: true

        ScrollView
        {
            visible: detailsLayout.horizontal

            padding: 20
            contentWidth: availableWidth
            contentItem.clip: false

            Layout.fillWidth: true
            Layout.preferredWidth: detailsLayout.width / 2
            Layout.fillHeight: detailsLayout.horizontal
            Layout.minimumHeight: 0
            Layout.minimumWidth: 0

            background: ScrollViewShadow { }

            LayoutItemProxy
            {
                width: parent.width
                target: detailsLayoutItem
            }
        }

        Rectangle
        {
            id: separator

            implicitWidth: 1

            visible: detailsLayout.horizontal
            color: ColorTheme.colors.dark9

            Layout.fillHeight: true
            Layout.topMargin: 20
            Layout.bottomMargin: 20
        }

        ScrollView
        {
            contentWidth: availableWidth
            contentItem.clip: false
            padding: 20

            Layout.fillWidth: true
            Layout.preferredWidth: detailsLayout.width / 2
            Layout.fillHeight: true

            background: ScrollViewShadow { }

            ColumnLayout
            {
                width: parent.width
                spacing: 20

                LayoutItemProxy
                {
                    target: detailsLayoutItem
                    visible: !detailsLayout.horizontal

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }

                Text
                {
                    id: descriptionText //< Id is required for the FT purposes.

                    text: NxGlobals.toHtmlWithLinks(objectData?.description ?? "")
                    visible: !!text

                    color: ColorTheme.colors.light10
                    font.pixelSize: 14
                    font.weight: Font.Normal
                    wrapMode: Text.Wrap

                    Layout.fillWidth: true

                    onLinkActivated: (link) =>
                    {
                        Workflow.openDialog("qrc:/qml/Nx/Web/LinkAboutToOpenDialog.qml", {"link": link});
                    }
                }

                MobileControls.TagView
                {
                    Layout.fillWidth: true

                    model: objectData?.tags ?? []
                    visible: hasTags
                    color: ColorTheme.colors.dark8
                }

                AnalyticsAttributeTable
                {
                    id: attributeTable

                    attributes: objectData?.attributes ?? []

                    visible: attributes && attributes.length > 0
                    interactive: true
                    highlightVisible: false
                    enableTooltip: false
                    nameFont.pixelSize: 14
                    nameColor: ColorTheme.colors.light10
                    valueFont.pixelSize: 14
                    valueColor: ColorTheme.colors.light6
                    valueFont.weight: Font.Medium
                    valueFont.underline: true
                    valueAlignment: Text.AlignRight
                    colorBoxSize: 12
                    colorBoxRadius: colorBoxSize / 2
                    colorBoxBorderColor: ColorTheme.colors.dark10
                    rowSpacing: 16
                    separatorsVisible: true

                    Layout.fillWidth: true

                    onValueClicked: (item) =>
                    {
                        searchSheet.attribute = item
                        searchSheet.open()
                    }

                    onContentChanged: searchSheet.close()
                }
            }
        }
    }

    ColumnLayout
    {
        id: detailsLayoutItem

        spacing: 20

        RemoteImage
        {
            id: previewImage

            readonly property real kAspectRatio: 9.0 / 16.0

            visible: eventDetailsScreen.showPreviewImage

            requestLine: eventDetailsScreen.showPreviewImage && objectData
                ? objectData.imagePath
                : ""

            Layout.fillWidth: true
            Layout.preferredHeight: width * kAspectRatio
        }

        Text
        {
            id: titleText

            text: objectData?.title ?? ""
            visible: !!text

            elide: Text.ElideRight
            wrapMode: Text.WrapAnywhere
            maximumLineCount: 2

            color: ColorTheme.colors.light4
            font.pixelSize: detailsLayout.horizontal ? 24 : 18
            font.weight: Font.Medium

            Layout.fillWidth: true
        }
    }

    MobileControls.AdaptiveSheet
    {
        id: searchSheet

        property var attribute: null

        title: qsTr("Search by %1").arg(attribute?.displayedName.toLowerCase() ?? "")

        Repeater
        {
            model: searchSheet.attribute?.displayedValues.length

            delegate: MobileControls.Button
            {
                readonly property var colorValue: searchSheet.attribute.colorValues[index]

                width: parent.width

                text: searchSheet.attribute.displayedValues[index]

                spacing: 8
                type: MobileControls.Button.LightInterface
                textHorizontalAlignment: colorValue ? Qt.AlignLeft : Qt.AlignHCenter
                icon.source: colorValue ? "image://skin/24x24/Solid/default_color.svg" : ""
                icon.color: colorValue ?? "transparent"

                onClicked:
                {
                    eventDetailsScreen.searchRequested(
                        EventSearchHelpers.createSearchRequestText(
                            searchSheet.attribute.id, [searchSheet.attribute.values[index]]))

                    searchSheet.close()
                }
            }
        }

        footer: MobileControls.Button
        {
            text: qsTr("Cancel")
            type: MobileControls.Button.LightInterface
            onClicked: searchSheet.close()
        }
    }

    onResourceChanged: d.updateCurrentEvent()
    onObjectDataChanged: d.updateCurrentEvent()

    QtObject
    {
        id: d

        readonly property bool portraitOrientation: eventDetailsScreen.width < eventDetailsScreen.height

        function updateCurrentEvent()
        {
            if (!objectData)
            {
                preview.interval.resource = null
                preview.interval.startTimeMs = 0
                preview.interval.durationMs = 0
                preview.interval.setPosition(0)
                return
            }

            preview.interval.stop()

            if (!eventDetailsScreen.resource)
                return;

            preview.interval.resource = eventDetailsScreen.resource

            const paddingTimeMs =
                eventDetailsScreen.objectsType === Timeline.ObjectsLoader.ObjectsType.analytics
                    ? CoreSettings.iniConfigValue("previewPaddingTimeMs")
                    : 0
            preview.interval.startTimeMs =
                objectData.startTimeMs - paddingTimeMs
            preview.interval.durationMs =
                objectData.durationMs + paddingTimeMs * 2
            preview.interval.setPosition(preview.interval.startTimeMs)

            preview.interval.play(preview.interval.startTimeMs) //< Loads the stream anyway.
        }

        function goToCamera()
        {
            Workflow.openVideoScreen(
                preview.interval.resource,
                undefined,
                preview.interval.startTimeMs,
                camerasModel,
                eventDetailsScreen.objectsType,
                /*isAuxiliary*/ true)
        }
    }

    Menu
    {
        id: menu

        width: 200

        readonly property bool available: downloadButton.enabled || shareButton.enabled

        MenuItem
        {
            id: downloadButton

            enabled: action.enabled && !preview.interval.cannotDecryptMedia

            action: DownloadMediaAction
            {
                resource: preview.interval.resource
                positionMs: preview.interval.startTimeMs
                durationMs: preview.interval.durationMs
            }
        }

        MenuItem
        {
            id: shareButton

            action: ShareAction
            {
                icon.width: 20
                icon.height: 20
                objectData: eventDetailsScreen.objectData
                analyticsMode:
                    eventDetailsScreen.objectsType === Timeline.ObjectsLoader.ObjectsType.analytics
            }
        }
    }
}
