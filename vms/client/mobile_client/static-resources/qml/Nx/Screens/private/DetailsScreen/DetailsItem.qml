// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Common
import Nx.Controls
import Nx.Core
import Nx.Core.Controls
import Nx.Core.EventSearch
import Nx.Core.Items
import Nx.Items
import Nx.Mobile.Controls as MobileControls
import Nx.Ui

import nx.vms.client.core
import nx.vms.client.mobile.timeline as Timeline

Item
{
    id: detailsItem

    readonly property string title: detailsItem.resource?.name ?? qsTr("Preview")
    property var uuid
    property int objectsType: Timeline.ObjectsLoader.ObjectsType.analytics
    required property Timeline.AbstractObjectData objectData
    required property Resource resource
    readonly property Menu menu: menu.available ? menu : null
    property alias hasNext: preview.hasNext
    property alias hasPrevious: preview.hasPrevious
    property alias withShowOnCamera: preview.withShowOnCamera
    property alias gestureExclusionEnabled: preview.gestureExclusionEnabled
    property bool showPreviewImage: false

    signal backClicked()
    signal searchRequested(string text)
    signal nextClicked()
    signal previousClicked()
    signal showOnCameraRequested(Resource resource, real timestampMs)

    clip: false

    ColumnLayout
    {
        anchors.fill: parent
        visible: d.isPortraitLayout

        LayoutItemProxy
        {
            Layout.fillWidth: true
            Layout.preferredHeight: LayoutController.fullscreen
                ? detailsItem.height
                : StyleHints.previewHeight

            target: preview
        }

        LayoutItemProxy
        {
            Layout.fillWidth: true
            Layout.fillHeight: true

            target: details
            visible: !LayoutController.fullscreen
        }
    }

    RowLayout
    {
        anchors.fill: parent
        visible: !d.isPortraitLayout

        LayoutItemProxy
        {
            Layout.fillWidth: true
            Layout.fillHeight: true

            target: preview
        }

        LayoutItemProxy
        {
            Layout.preferredWidth: StyleHints.panelWidth
            Layout.fillHeight: true

            target: details
            visible: !LayoutController.fullscreen
        }
    }

    Preview
    {
        id: preview

        title: detailsItem.title

        onNext: detailsItem.nextClicked()
        onPrevious: detailsItem.previousClicked()
        onBack: detailsItem.backClicked()
        onShowOnCamera:
        {
            detailsItem.showOnCameraRequested(
                interval.resource, detailsItem.objectData?.startTimeMs ?? 0)
        }
    }

    ScrollView
    {
        id: details

        contentWidth: availableWidth
        contentItem.clip: false
        padding: 20
        clip: true

        background: ScrollViewShadow { }

        ColumnLayout
        {
            width: parent.width
            spacing: 20

            RemoteImage
            {
                id: previewImage

                readonly property real kAspectRatio: 9.0 / 16.0

                visible: detailsItem.showPreviewImage

                requestLine: detailsItem.showPreviewImage && objectData
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
                font.pixelSize: d.isPortraitLayout ? 18 : 24
                font.weight: Font.Medium

                Layout.fillWidth: true
            }

            Text
            {
                id: descriptionText //< Id is required for the FT purposes.

                text: NxGlobals.toHtmlWithLinks(objectData?.description ?? "")
                visible: !!text

                color: ColorTheme.colors.light10
                linkColor: ColorTheme.colors.brand_core
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
                    detailsItem.searchRequested(
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

    ChunkProvider
    {
        id: archiveProvider

        onLoadingChanged: d.evaluateArchive()
        onPeriodsUpdated: (contentType) =>
        {
            if (contentType === ChunkProvider.RecordingContent)
                d.evaluateArchive()
        }
    }

    QtObject
    {
        id: d

        readonly property bool isPortraitLayout: detailsItem.width < detailsItem.height

        function updateCurrentEvent()
        {
            if (!objectData)
            {
                preview.interval.resource = null
                preview.interval.startTimeMs = 0
                preview.interval.durationMs = 0
                preview.interval.setPosition(0)
                archiveProvider.resource = null
                preview.dataState = Preview.DataState.Checking
                return
            }

            preview.interval.stop()
            preview.interval.startTimeMs = 0
            preview.interval.durationMs = 0

            if (!detailsItem.resource)
            {
                archiveProvider.resource = null
                return
            }

            preview.interval.resource = detailsItem.resource

            preview.dataState = Preview.DataState.Checking
            archiveProvider.resource = detailsItem.resource
            evaluateArchive()
        }

        function startPlayback()
        {
            const paddingTimeMs =
                detailsItem.objectsType === Timeline.ObjectsLoader.ObjectsType.analytics
                    ? CoreSettings.iniConfigValue("previewPaddingTimeMs")
                    : 0
            preview.interval.startTimeMs =
                objectData.startTimeMs - paddingTimeMs
            preview.interval.durationMs =
                objectData.durationMs + paddingTimeMs * 2
            preview.interval.setPosition(preview.interval.startTimeMs)

            preview.interval.play(preview.interval.startTimeMs) //< Loads the stream anyway.
        }

        function evaluateArchive()
        {
            if (!archiveProvider.resource || !objectData)
                return

            if (archiveProvider.hasArchive(objectData.startTimeMs))
            {
                if (preview.dataState !== Preview.DataState.Available)
                {
                    preview.dataState = Preview.DataState.Available
                    startPlayback()
                }
            }
            else if (!archiveProvider.loading)
            {
                preview.dataState = Preview.DataState.NoData
            }
            else
            {
                preview.dataState = Preview.DataState.Checking
            }
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

            enabled: detailsItem.objectsType !== Timeline.ObjectsLoader.ObjectsType.motion
                && action.enabled
                && !preview.interval.cannotDecryptMedia

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
                objectData: detailsItem.objectData
                objectsType: detailsItem.objectsType
            }
        }
    }
}
