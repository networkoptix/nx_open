// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.14

import Nx 1.0
import Nx.Core 1.0
import Nx.Analytics 1.0
import Nx.Controls 1.0
import Nx.RightPanel 1.0

import nx.vms.client.core 1.0
import nx.vms.client.desktop 1.0
import nx.vms.client.desktop.analytics 1.0 as Analytics

import ".."

import "../metrics.js" as Metrics

Item
{
    id: analyticsPanel

    property alias active: eventModel.active
    property alias model: eventModel

    property int defaultCameraSelection: { return RightPanel.CameraSelection.layout }

    property alias showInformation: counterBlock.showInformation
    property alias showThumbnails: counterBlock.showThumbnails

    property bool resizeEnabled: true

    property real desiredFiltersWidth: Metrics.kDefaultFilterColumnWidth
    property real maximumWidth: Number.MAX_VALUE

    readonly property real maximumFiltersWidth: Math.min(Metrics.kMaximumFilterColumnWidth,
        analyticsPanel.maximumWidth - Metrics.kFixedSearchPanelWidth - resultsPanelResizer.width)

    readonly property real filtersWidth: MathUtils.bound(
        Metrics.kMinimumFilterColumnWidth,
        analyticsPanel.desiredFiltersWidth,
        analyticsPanel.maximumFiltersWidth)

    property int desiredResultColumns: Metrics.kDefaultObjectSearchColumns

    readonly property real maximumResultsWidth:
        analyticsPanel.maximumWidth - analyticsPanel.filtersWidth - resultsPanelResizer.width

    readonly property int maximumResultColumns: Math.max(1, Math.floor(
        ((analyticsPanel.maximumResultsWidth - eventGrid.leftMargin - eventGrid.rightMargin)
            / eventGrid.cellWidth)))

    readonly property int resultColumns: MathUtils.bound(
        1, analyticsPanel.desiredResultColumns, analyticsPanel.maximumResultColumns)

    implicitWidth: analyticsPanel.filtersWidth + resultsPanel.width + resultsPanelResizer.width

    signal filtersReset()

    Scrollable
    {
        id: filtersPanel

        width: analyticsPanel.filtersWidth - scrollBar.width
        height: analyticsPanel.height

        verticalScrollBar: scrollBar

        contentItem: SearchPanelHeader
        {
            id: header

            model: eventModel

            width: filtersPanel.width
            height: implicitHeight

            SelectableTextButton
            {
                id: areaSelectionButton

                parent: header.filtersColumn
                width: Math.min(implicitWidth, header.filtersColumn.width)

                selectable: false
                icon.source: "image://svg/skin/text_buttons/frame_20x20_deprecated.svg"
                accented: eventModel.analyticsSetup.areaSelectionActive

                desiredState: (eventModel.analyticsSetup.isAreaSelected
                    || eventModel.analyticsSetup.areaSelectionActive)
                        ? SelectableTextButton.Unselected
                        : SelectableTextButton.Deactivated

                text:
                {
                    if (state === SelectableTextButton.Deactivated)
                        return qsTr("Select area")

                    return accented
                        ? qsTr("Select some area on the video...")
                        : qsTr("In selected area")
                }

                onDeactivated:
                    eventModel.analyticsSetup.clearAreaSelection()

                onClicked:
                    eventModel.analyticsSetup.areaSelectionActive = true
            }

            AnalyticsFilters
            {
                id: analyticsFilters

                width: parent.width
                bottomPadding: 16

                model: windowContext.systemContext.taxonomyManager()
                    .createFilterModel(analyticsPanel)

                Connections
                {
                    target: eventModel.commonSetup
                    function onSelectedCamerasChanged()
                    {
                        analyticsFilters.model.setSelectedDevices(
                            eventModel.commonSetup.selectedCameras)
                    }
                }
            }

            onFiltersReset:
            {
                analyticsFilters.clear()
                eventModel.commonSetup.cameraSelection = analyticsPanel.defaultCameraSelection
            }
        }
    }

    ScrollBar
    {
        id: scrollBar

        height: analyticsPanel.height
        anchors.left: filtersPanel.right
        policy: ScrollBar.AlwaysOn
        enabled: size < 1
        opacity: enabled ? 1.0 : 0.1
    }

    Resizer
    {
        id: filtersPanelResizer

        z: 1
        edge: Qt.RightEdge
        target: filtersPanel
        enabled: analyticsPanel.resizeEnabled
        handleWidth: Metrics.kResizerWidth
        drag.minimumX: Metrics.kMinimumFilterColumnWidth
        drag.maximumX: Math.min(Metrics.kMaximumFilterColumnWidth,
            analyticsPanel.maximumWidth - resultsPanel.width - resultsPanelResizer.width)

        anchors.leftMargin: scrollBar.width

        onDragPositionChanged:
            analyticsPanel.desiredFiltersWidth = pos
    }

    Column
    {
        id: resultsPanel

        anchors.left: scrollBar.right

        width: eventGrid.width
        height: analyticsPanel.height

        CounterBlock
        {
            id: counterBlock

            width: resultsPanel.width

            displayedItemsText: eventModel.itemCountText

            availableItemsText:
            {
                const count = eventModel.analyticsSetup.availableNewTracks
                if (!count)
                    return ""

                const newResults = count > 0
                    ? qsTr("%n new results", "", count)
                    : qsTr("new results")

                return `+ ${newResults}`
            }

            showInformationButton: true
            showRemoveColumn: analyticsPanel.resultColumns > 1
            showAddColumn: analyticsPanel.resultColumns < analyticsPanel.maximumResultColumns

            onRemoveColumnClicked:
                analyticsPanel.desiredResultColumns = analyticsPanel.resultColumns - 1

            onAddColumnClicked:
                analyticsPanel.desiredResultColumns = analyticsPanel.resultColumns + 1

            onCommitNewItemsRequested:
                commitAvailableNewTracks()

            onAvailableItemsTextChanged:
            {
                // Automatically commit available new tracks once if the results grid is empty.
                if (!eventGrid.count && availableItemsText)
                    commitAvailableNewTracks()
            }

            function commitAvailableNewTracks()
            {
                eventGrid.positionViewAtBeginning()
                eventModel.analyticsSetup.commitAvailableNewTracks()
            }
        }

        EventGrid
        {
            id: eventGrid

            objectName: analyticsPanel.objectName + ".EventGrid"

            placeholder
            {
                icon: "image://svg/skin/left_panel/placeholders/objects.svg"
                title: qsTr("No objects")
                description: qsTr("Try changing the filters or configure object detection in the"
                    + " camera plugin settings")
            }

            tileController
            {
                showInformation: analyticsPanel.showInformation
                showThumbnails: analyticsPanel.showThumbnails
                videoPreviewMode: RightPanelGlobals.VideoPreviewMode.none
            }

            columnWidth: Metrics.kTileWidth

            width: eventGrid.leftMargin + eventGrid.rightMargin
                + (analyticsPanel.resultColumns * eventGrid.cellWidth)

            height: analyticsPanel.height - counterBlock.height

            model: RightPanelModel
            {
                id: eventModel

                context: windowContext
                type: { return RightPanelModel.Type.analytics }
                previewsEnabled: analyticsPanel.showThumbnails

                Component.onCompleted:
                    commonSetup.cameraSelection = analyticsPanel.defaultCameraSelection
            }

            onContentYChanged:
                toolTip.suppress(/*immediately*/ true)

            AnalyticsToolTip
            {
                id: toolTip

                property alias tile: eventGrid.hoveredItem

                onTileChanged:
                {
                    if (!tile)
                    {
                        hide()
                        return
                    }

                    enclosingRect = Qt.rect(
                        tile.width,
                        tile.mapFromItem(analyticsPanel, 0, 0).y,
                        Number.MAX_VALUE / 2,
                        analyticsPanel.height)

                    target = tile
                    previewState = tile.previewState
                    previewAspectRatio = tile.previewAspectRatio
                    videoPreviewTimestampMs = tile.videoPreviewTimestampMs
                    videoPreviewResourceId = tile.videoPreviewResourceId
                    attributeItems = analyticsPanel.showInformation ? [] : tile.attributeItems
                    footerText = getFooterText(tile.getData("analyticsEngineName"))

                    if (state !== BubbleToolTip.Suppressed)
                        show()
                }

                function getFooterText(engineName)
                {
                    if (!engineName)
                        return ""

                    const detectedBy = qsTr("Detected by")
                    return `<div style="font-weight: 200;">${detectedBy}</div> ${engineName}`
                }
            }
        }
    }

    Resizer
    {
        id: resultsPanelResizer

        z: 1
        edge: Qt.RightEdge
        target: resultsPanel
        enabled: analyticsPanel.resizeEnabled
        cursorShape: Qt.SizeHorCursor
        handleWidth: Metrics.kResizerWidth

        onDragPositionChanged:
        {
            const nearestResultColumns = Math.round(
                ((pos - resultsPanel.x - eventGrid.leftMargin - eventGrid.rightMargin)
                    / eventGrid.cellWidth))

            const newResultColumns = MathUtils.bound(
                1, nearestResultColumns, analyticsPanel.maximumResultColumns)

            if (newResultColumns !== analyticsPanel.resultColumns)
                analyticsPanel.desiredResultColumns = newResultColumns
        }
    }

    NxObject
    {
        id: d

        property var delayedAttributesFilter: []

        PropertyUpdateFilter on delayedAttributesFilter
        {
            source: analyticsFilters.selectedAttributeFilters
            minimumIntervalMs: 250
        }

        Binding
        {
            target: eventModel.analyticsSetup
            property: "attributeFilters"
            value: d.delayedAttributesFilter
        }

        Binding
        {
            target: eventModel.analyticsSetup
            property: "objectTypes"
            value: analyticsFilters.selectedAnalyticsObjectTypeIds
        }
    }
}
