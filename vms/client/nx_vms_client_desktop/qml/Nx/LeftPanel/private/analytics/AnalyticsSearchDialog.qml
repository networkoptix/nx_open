// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Window

import Nx
import Nx.Core
import Nx.Analytics
import Nx.Controls
import Nx.Models
import Nx.RightPanel

import nx.client.desktop
import nx.vms.client.core
import nx.vms.client.desktop
import nx.vms.client.desktop.analytics as Analytics

import ".."

import "../metrics.js" as Metrics

Window
{
    id: dialog

    property WindowContext windowContext: null

    modality: Qt.NonModal

    width: Metrics.kDefaultDialogWidth
    height: Metrics.kDefaultDialogHeight
    minimumHeight: Metrics.kMinimumDialogHeight

    minimumWidth: Math.max(Metrics.kMinimumDialogWidth,
        filtersPanel.width + previewPanel.width + Metrics.kSearchResultsWidth + 2)

    onWidthChanged:
    {
        previewPanel.setWidth(previewPanel.width)
    }

    color: ColorTheme.colors.dark3

    title: qsTr("Advanced Object Search")

    onVisibleChanged:
    {
        // Always load new tiles when the dialog is reopened.
        if (dialog.visible)
            counterBlock.commitAvailableNewTracks()
    }

    onTransientParentChanged:
    {
        if (visible)
        {
            // Reopen the dialog to apply the new transient parent.
            hide()
            show()
        }
    }

    signal accepted()

    TabBar
    {
        id: tabBar

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top

        height: d.pluginTabsShown ? 36 : 0

        color: ColorTheme.colors.dark9
        visible: d.pluginTabsShown

        leftPadding: 16
        rightPadding: 16
        topPadding: 1
        spacing: 16

        function selectEngine(engineId)
        {
            for (let i = 0; i < tabBar.count; ++i)
            {
                if (tabBar.itemAt(i).engineId === engineId)
                {
                    tabBar.currentIndex = i
                    return
                }
            }
        }

        component EngineButton: CompactTabButton
        {
            property Analytics.Engine engine: null

            readonly property var engineId: NxGlobals.uuid(engine ? engine.id : "")
            text: engine ? engine.name : qsTr("Any plugin")
            compact: false

            textHeight: 13
            topPadding: 14
            underlineHeight: 2
            inactiveUnderlineColor: "transparent"
            font: Qt.font({pixelSize: 12, weight: Font.Normal})
        }

        EngineButton {}

        Repeater
        {
            model: d.filterModel.engines

            onModelChanged:
            {
                if (d.filterModel.engines.length > 0 && eventModel.analyticsSetup)
                    tabBar.selectEngine(eventModel.analyticsSetup.engine)
            }

            EngineButton
            {
                engine: modelData
            }
        }
    }

    Item
    {
        id: content

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.top: tabBar.bottom

        Rectangle
        {
            id: filtersPanel

            width: Metrics.kMinimumFilterColumnWidth
            height: content.height
            color: ColorTheme.colors.dark4

            Scrollable
            {
                id: filtersContainer

                width: parent.width - scrollBar.width - 2
                height: parent.height

                verticalScrollBar: scrollBar

                contentItem: SearchPanelHeader
                {
                    id: header

                    model: eventModel

                    y: 12
                    width: filtersContainer.width
                    height: implicitHeight

                    SelectableTextButton
                    {
                        id: areaSelectionButton

                        parent: header.filtersColumn
                        width: Math.min(implicitWidth, header.filtersColumn.width)

                        selectable: false
                        icon.source: "image://svg/skin/text_buttons/frame_20x20_deprecated.svg"
                        accented: eventModel.analyticsSetup
                            && eventModel.analyticsSetup.areaSelectionActive

                        desiredState:
                            eventModel.analyticsSetup && (eventModel.analyticsSetup.isAreaSelected
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
                        {
                            if (eventModel.analyticsSetup)
                                eventModel.analyticsSetup.clearAreaSelection()
                        }

                        onClicked:
                        {
                            if (eventModel.analyticsSetup)
                               eventModel.analyticsSetup.areaSelectionActive = true
                        }
                    }

                    AnalyticsFilters
                    {
                        id: analyticsFilters

                        width: parent.width
                        bottomPadding: 16

                        model: d.filterModel
                    }

                    onFiltersReset:
                    {
                        analyticsFilters.clear()
                        eventModel.commonSetup.cameraSelection = RightPanel.CameraSelection.layout
                    }
                }
            }

            ScrollBar
            {
                id: scrollBar

                height: content.height
                anchors.left: filtersContainer.right
                policy: ScrollBar.AlwaysOn
                enabled: size < 1
                opacity: enabled ? 1.0 : 0.1
            }

            Rectangle
            {
                anchors.right: parent.right

                width: 1
                height: parent.height
                color: ColorTheme.colors.dark9
            }
        }

        Resizer
        {
            id: filtersPanelResizer

            z: 1
            edge: Qt.RightEdge
            target: filtersPanel
            handleWidth: Metrics.kResizerWidth
            drag.minimumX: Metrics.kMinimumFilterColumnWidth

            drag.onActiveChanged:
            {
                if (drag.active)
                {
                    drag.maximumX = Math.min(Metrics.kMaximumFilterColumnWidth,
                        filtersPanel.width + resultsPanel.width - Metrics.kMinimumSearchPanelWidth)
                }
            }

            onDragPositionChanged:
                filtersPanel.width = pos
        }

        Column
        {
            id: resultsPanel

            anchors.left: filtersPanel.right

            width:
            {
                let resultsWidth = content.width - filtersPanel.width - 1

                // Reduce resultsPanel width only when previewPanel is fully visible.
                // Avoid resource-consuming grid resize animation.
                if (eventGrid.count > 0
                    && counterBlock.showPreview
                    && !previewPanel.slideAnimationEnabled)
                {
                    resultsWidth -= previewPanel.width
                }

                return Math.max(resultsWidth, Metrics.kMinimumSearchPanelWidth)
            }

            height: content.height

            CounterBlock
            {
                id: counterBlock

                width: resultsPanel.width

                property int availableNewTracks: eventModel.analyticsSetup
                    ? eventModel.analyticsSetup.availableNewTracks : 0

                displayedItemsText: eventModel.itemCountText

                availableItemsText:
                {
                    if (!availableNewTracks)
                        return ""

                    const newResults = availableNewTracks > 0
                        ? qsTr("%n new results", "", availableNewTracks)
                        : qsTr("new results")

                    return `+ ${newResults}`
                }

                showThumbnailsButton: false

                onShowPreviewChanged:
                    previewPanel.slideAnimationEnabled = true

                onCommitNewItemsRequested:
                    commitAvailableNewTracks()

                onAvailableNewTracksChanged:
                {
                    // Automatically commit available new tracks once if the results grid is empty.
                    if (!eventGrid.count && availableNewTracks)
                        Qt.callLater(commitAvailableNewTracks)
                }

                function commitAvailableNewTracks()
                {
                    eventGrid.positionViewAtBeginning()
                    if (eventModel.analyticsSetup)
                        eventModel.analyticsSetup.commitAvailableNewTracks()
                    if (eventModel.itemCount > 0)
                        selection.index = eventModel.index(0, 0)
                }
            }

            Rectangle
            {
                width: resultsPanel.width
                height: 1
                color: ColorTheme.colors.dark6
            }

            EventGrid
            {
                id: eventGrid

                objectName: "AnalyticsSearchDialog.EventGrid"
                standardTileInteraction: false
                keyNavigationEnabled: false //< We implement our own.
                focus: true

                currentIndex: selection.index.row

                placeholder
                {
                    icon: "image://svg/skin/left_panel/placeholders/objects.svg"
                    title: qsTr("No objects")
                    description: qsTr("Try changing the filters or configure object detection "
                        + "in the camera plugin settings")
                }

                tileController
                {
                    showInformation: false
                    showThumbnails: true
                    selectedRow: selection.index.row

                    videoPreviewMode: counterBlock.showPreview
                        ? RightPanelGlobals.VideoPreviewMode.none
                        : RightPanelGlobals.VideoPreviewMode.selection

                    onClicked: row =>
                    {
                        eventGrid.forceActiveFocus()

                        if (row !== selection.index.row)
                            selection.index = eventModel.index(row, 0)
                    }

                    onDoubleClicked:
                        d.showSelectionOnLayout()
                }

                Item
                {
                    // Single item which is re-parented to the selected tile overlay.
                    id: tilePreviewOverlay

                    parent: eventGrid.tileController.selectedTile
                        && eventGrid.tileController.selectedTile.overlayContainer

                    anchors.fill: parent || undefined
                    visible: parent && !counterBlock.showPreview

                    Column
                    {
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.margins: 4
                        spacing: 4

                        TileOverlayButton
                        {
                            icon.source: "image://svg/skin/advanced_search_dialog/show_on_layout.svg"
                            accent: true

                            onClicked:
                                d.showSelectionOnLayout()
                        }
                    }
                }

                PersistentIndexWatcher
                {
                    id: selection
                }

                ModelDataAccessor
                {
                    id: accessor
                    model: eventGrid.model
                }

                InformationBubble
                {
                    id: informationToolTip

                    view: counterBlock.showPreview ? null : eventGrid
                    z: 2
                }

                readonly property real availableWidth: width - leftMargin - rightMargin

                readonly property int numColumns: Math.floor(
                    availableWidth / (Metrics.kMinimumTileWidth + columnSpacing))

                readonly property int rowsPerPage:
                    Math.floor((height - topMargin - bottomMargin) / cellHeight)

                columnWidth: (availableWidth / numColumns) - columnSpacing

                width: resultsPanel.width
                height: resultsPanel.height - counterBlock.height

                model: RightPanelModel
                {
                    id: eventModel

                    context: windowContext
                    type: { return RightPanelModel.Type.analytics }
                    previewsEnabled: counterBlock.showThumbnails
                    active: true

                    Component.onCompleted:
                        commonSetup.cameraSelection = RightPanel.CameraSelection.layout

                    onAnalyticsSetupChanged:
                    {
                        const engineId = analyticsSetup.engine
                        tabBar.selectEngine(engineId)

                        analyticsFilters.setSelectedAnalyticsObjectTypeIds(
                            analyticsSetup.objectTypes)

                        analyticsFilters.setSelectedAttributeFilters(
                            analyticsSetup.attributeFilters)
                    }
                }

                Shortcut
                {
                    sequence: "Home"
                    enabled: eventGrid.count > 0

                    onActivated:
                        selection.index = eventModel.index(0, 0)
                }

                Shortcut
                {
                    sequence: "End"
                    enabled: eventGrid.count > 0

                    onActivated:
                        selection.index = eventModel.index(eventModel.rowCount() - 1, 0)
                }

                Shortcut
                {
                    sequence: "Left"
                    enabled: eventGrid.count > 0 && selection.index.valid

                    onActivated:
                    {
                        const newRow = Math.max(selection.index.row - 1, 0)
                        selection.index = eventModel.index(newRow, 0)
                    }
                }

                Shortcut
                {
                    sequence: "Right"
                    enabled: eventGrid.count > 0 && selection.index.valid

                    onActivated:
                    {
                        const newRow = Math.min(selection.index.row + 1, eventModel.rowCount() - 1)
                        selection.index = eventModel.index(newRow, 0)
                    }
                }

                Shortcut
                {
                    sequence: "Up"
                    enabled: eventGrid.count > 0 && selection.index.valid

                    onActivated:
                    {
                        const newRow = selection.index.row - eventGrid.numColumns
                        if (newRow >= 0)
                            selection.index = eventModel.index(newRow, 0)
                    }
                }

                Shortcut
                {
                    sequence: "Down"
                    enabled: eventGrid.count > 0 && selection.index.valid

                    onActivated:
                    {
                        const newRow = selection.index.row + eventGrid.numColumns
                        if (newRow < eventModel.rowCount())
                            selection.index = eventModel.index(newRow, 0)
                    }
                }

                Shortcut
                {
                    sequence: "PgUp"
                    enabled: eventGrid.count > 0 && selection.index.valid

                    onActivated:
                    {
                        const newRow = Math.max(0,
                            selection.index.row - eventGrid.rowsPerPage * eventGrid.numColumns)
                        selection.index = eventModel.index(newRow, 0)
                    }
                }

                Shortcut
                {
                    sequence: "PgDown"
                    enabled: eventGrid.count > 0 && selection.index.valid

                    onActivated:
                    {
                        const newRow = Math.min(eventModel.rowCount() - 1,
                            selection.index.row + eventGrid.rowsPerPage * eventGrid.numColumns)
                        selection.index = eventModel.index(newRow, 0)
                    }
                }

                Shortcut
                {
                    sequences: ["Up", "Down", "Left", "Right", "PgUp", "PgDown"]
                    enabled: eventGrid.count > 0 && !selection.index.valid

                    onActivated:
                        selection.index = eventModel.index(0, 0)
                }
            }
        }

        Resizer
        {
            id: previewPanelResizer

            z: 1
            edge: Qt.LeftEdge
            target: previewPanel
            anchors.rightMargin: -width //< Place the resizer insize previewPanel.

            enabled: counterBlock.showPreview

            onDragPositionChanged:
            {
                const requestedWidth = previewPanel.x + previewPanel.width - pos - anchors.rightMargin
                previewPanel.setWidth(requestedWidth)
            }
        }

        PreviewPanel
        {
            id: previewPanel

            height: content.height
            width: 460
            x: counterBlock.showPreview ? content.width - width : content.width

            visible: eventGrid.count > 0

            prevEnabled: selection.index.row > 0
            nextEnabled: selection.index.row != -1 && selection.index.row < accessor.count - 1

            onPrevClicked:
                selection.index = eventModel.index(selection.index.row - 1, 0)

            onNextClicked:
                selection.index = eventModel.index(selection.index.row + 1, 0)

            onShowOnLayoutClicked:
                d.showSelectionOnLayout()

            onSearchRequested: (attributeRow) =>
            {
                const rawAttributes = accessor.getData(selection.index, "rawAttributes")
                const attribute = rawAttributes[attributeRow]
                if (attribute)
                    header.searchText = createSearchRequestText(attribute.id, attribute.values)
            }

            selectedItem:
            {
                if (selection.index.row == -1 || !counterBlock.showPreview)
                    return null

                const getData = name => accessor.getData(selection.index, name)

                const previewResource = getData("previewResource")

                return {
                    "previewResourceId": (previewResource && previewResource.id)
                        || NxGlobals.uuid(""),
                    "previewTimestampMs": getData("previewTimestampMs") || 0,
                    "previewAspectRatio": getData("previewAspectRatio") || 1.0,
                    "timestamp": getData("timestamp") || "",
                    "display": getData("display") || "",
                    "description": getData("description") || "",
                    "additionalText": getData("additionalText") || "",
                    "attributes": getData("attributes") || [],
                    "resourceList": getData("resourceList") || []
                }
            }

            function createSearchRequestText(key, values)
            {
                const escape =
                    (str) =>
                    {
                        str = str.replace(/([\"\\\:\$])/g, '\\$1');
                        return str.includes(" ") ? `"${str}"` : str
                    }

                const escapedKeyValuePairs =
                    Array.prototype.map.call(values, v => `${escape(key)}=${escape(v)}`)

                return escapedKeyValuePairs.join(" ")
            }

            function setWidth(requestedWidth)
            {
                const maxWidth = dialog.width - filtersPanel.width - Metrics.kSearchResultsWidth
                previewPanel.width = Math.max(Math.min(requestedWidth, maxWidth), 280)
            }
        }
    }

    NxObject
    {
        id: d

        property bool isModelEmpty: true

        property var analyticsFiltersByEngine: ({})

        property var filterModel: windowContext
            ? windowContext.systemContext.taxonomyManager().createFilterModel(dialog)
            : emptyFilterModel

        readonly property Analytics.Engine selectedAnalyticsEngine:
            tabBar.currentItem ? tabBar.currentItem.engine : null

        property var delayedAttributesFilter: []

        readonly property bool pluginTabsShown: filterModel.engines.length > 1

        Analytics.FilterModel { id: emptyFilterModel }

        PropertyUpdateFilter on delayedAttributesFilter
        {
            source: analyticsFilters.selectedAttributeFilters
            minimumIntervalMs: 250
        }

        function showSelectionOnLayout()
        {
            dialog.transientParent = null //< Allows to raise the main window.
            if (selection.index.valid)
                eventModel.showOnLayout(selection.index.row)
        }

        onSelectedAnalyticsEngineChanged:
        {
            if (filterModel.engines.length === 0)
                return

            storeCurrentEngineFilterState()

            eventModel.analyticsSetup.engine = selectedAnalyticsEngine
                ? NxGlobals.uuid(selectedAnalyticsEngine.id)
                : NxGlobals.uuid("")

            restoreEngineFilterState(selectedAnalyticsEngine)
        }

        onDelayedAttributesFilterChanged:
        {
            if (!eventModel.analyticsSetup)
                return

            eventModel.analyticsSetup.attributeFilters = delayedAttributesFilter
        }

        Connections
        {
            target: eventModel.commonSetup
            function onSelectedCamerasChanged()
            {
                d.filterModel.setSelectedDevices(
                    eventModel.commonSetup.selectedCameras)
            }
        }

        Connections
        {
            target: analyticsFilters
            enabled: eventModel.analyticsSetup
            function onSelectedAnalyticsObjectTypeIdsChanged()
            {
                eventModel.analyticsSetup.objectTypes =
                    analyticsFilters.selectedAnalyticsObjectTypeIds
            }
        }

        function storeCurrentEngineFilterState()
        {
            analyticsFiltersByEngine[analyticsFilters.engine] = {
                objectTypeIds: analyticsFilters.selectedAnalyticsObjectTypeIds,
                attributeFilters: analyticsFilters.selectedAttributeFilters
            }
        }

        function restoreEngineFilterState(engine)
        {
            const savedData = analyticsFiltersByEngine[engine]

            analyticsFilters.setSelected(
                engine,
                savedData ? savedData.objectTypeIds : null,
                savedData ? savedData.attributeFilters : {})
        }

        Connections
        {
            target: eventModel.analyticsSetup

            function onObjectTypesChanged()
            {
                analyticsFilters.setSelectedAnalyticsObjectTypeIds(
                    eventModel.analyticsSetup.objectTypes)
            }

            function onAttributeFiltersChanged()
            {
                analyticsFilters.setSelectedAttributeFilters(
                    eventModel.analyticsSetup.attributeFilters)
            }

            function onEngineChanged()
            {
                const engineId = eventModel.analyticsSetup.engine
                tabBar.selectEngine(engineId)
            }
        }

        Connections
        {
            target: eventModel

            function onItemCountChanged()
            {
                const hasItems = eventModel.itemCount > 0
                if (hasItems && d.isModelEmpty)
                    selection.index = eventModel.index(0, 0)

                d.isModelEmpty = !hasItems
            }
        }
    }
}
