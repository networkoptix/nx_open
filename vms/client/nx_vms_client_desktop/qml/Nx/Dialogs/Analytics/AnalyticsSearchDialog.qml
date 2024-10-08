// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts
import QtQuick.Window

import Nx.Analytics
import Nx.Controls
import Nx.Core
import Nx.Core.Controls
import Nx.Models
import Nx.RightPanel

import nx.vms.client.core
import nx.vms.client.core.analytics as Analytics
import nx.vms.client.desktop
import nx.vms.client.desktop.analytics

import "metrics.js" as Metrics
import "../../RightPanel/private" as RightPanel

Window
{
    id: dialog

    property WindowContext windowContext: null
    property bool showPreview: false
    property alias tileView: tileViewButton.checked

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

    ContextHelp.topicId: HelpTopic.ObjectSearch

    onVisibleChanged:
    {
        // Always load new tiles when the dialog is reopened.
        if (dialog.visible)
            counterBlock.commitAvailableNewTracks()
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

        component EngineButton: PrimaryTabButton
        {
            property Analytics.Engine engine: null
            readonly property var engineId: NxGlobals.uuid(engine ? engine.id : "")

            text: engine ? engine.name : qsTr("Any plugin")
            backgroundColors.pressed: ColorTheme.colors.dark8
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
                    searchDelay: LocalSettings.iniConfigValue("analyticsSearchRequestDelayMs")

                    y: 12
                    width: filtersContainer.width
                    height: implicitHeight

                    SelectableTextButton
                    {
                        id: areaSelectionButton

                        parent: header.filtersColumn
                        Layout.maximumWidth: header.filtersColumn.width

                        selectable: false
                        icon.source: "image://skin/20x20/Outline/frame.svg"
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

            onDragPositionChanged: (pos) =>
                filtersPanel.width = pos
        }

        ColumnLayout
        {
            id: resultsPanel

            anchors.left: filtersPanel.right
            spacing: 0
            width:
            {
                let resultsWidth = content.width - filtersPanel.width - 1

                // Reduce resultsPanel width only when previewPanel is fully visible.
                // Avoid resource-consuming grid resize animation.
                if (eventGrid.count > 0
                    && showPreview
                    && !previewPanel.slideAnimationEnabled)
                {
                    resultsWidth -= previewPanel.width
                }

                return Math.max(resultsWidth, Metrics.kMinimumSearchPanelWidth)
            }

            height: content.height

            RowLayout
            {
                Layout.minimumHeight: Metrics.kTopBarHeight
                Layout.leftMargin: 8
                Layout.rightMargin: 8
                spacing: 8

                visible: !eventModel.placeholderRequired

                CounterBlock
                {
                    id: counterBlock

                    Layout.alignment: Qt.AlignVCenter

                    property int availableNewTracks: eventModel.analyticsSetup
                        ? eventModel.analyticsSetup.availableNewTracks : 0

                    displayedItemsText: eventModel.itemCountText
                    visible: !!displayedItemsText

                    availableItemsText:
                    {
                        if (!availableNewTracks)
                            return ""

                        const newResults = availableNewTracks > 0
                            ? qsTr("%n new results", "", availableNewTracks)
                            : qsTr("new results")

                        return `+ ${newResults}`
                    }

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

                TextButton
                {
                    id: settingsButton

                    Layout.alignment: Qt.AlignVCenter
                    text: qsTr("Settings")
                    visible: d.objectTypeSelected
                    icon.source: "image://skin/20x20/Outline/settings.svg"

                    onClicked:
                    {
                        if (tileView)
                            tileFilterSettingsDialog.show()
                        else
                            tableFilterSettingsDialog.show()
                    }
                }

                Item
                {
                    Layout.fillWidth: true
                }

                ImageButton
                {
                    id: tileViewButton

                    Layout.alignment: Qt.AlignVCenter
                    padding: 6
                    radius: 4

                    checkable: true
                    checked: true

                    icon.source: checked
                        ? "image://skin/20x20/Outline/table_view.svg"
                        : "image://skin/20x20/Outline/card_view.svg"
                    borderColor: ColorTheme.button
                }
            }

            Rectangle
            {
                height: 1
                color: ColorTheme.colors.dark6
            }

            StackLayout
            {
                id: layout
                currentIndex: dialog.tileView ? 0 : 1
                Layout.fillHeight: true

                EventGrid
                {
                    id: eventGrid

                    objectName: "AnalyticsSearchDialog.EventGrid"
                    standardTileInteraction: false
                    keyNavigationEnabled: false //< We implement our own.
                    focus: true

                    currentIndex: selection.index.row
                    controller.active: tileView

                    placeholder
                    {
                        icon: "image://skin/64x64/Outline/noobjects.svg?primary=dark17"
                        title: qsTr("No objects")
                        description: qsTr("Try changing the filters or configure object detection "
                            + "in the camera plugin settings")
                    }

                    tileController
                    {
                        showInformation: d.objectTypeSelected
                        showThumbnails: true
                        selectedRow: dialog.showPreview ? selection.index.row : -1
                        attributeManager: d.tileViewAttributeManager

                        videoPreviewMode: showPreview
                            ? RightPanelGlobals.VideoPreviewMode.none
                            : RightPanelGlobals.VideoPreviewMode.selection

                        onClicked: row =>
                        {
                            if (!showPreview)
                            {
                                previewPanel.slideAnimationEnabled = true
                                showPreview = true
                            }
                            eventGrid.forceActiveFocus()
                            if (row !== selection.index.row)
                                selection.index = eventModel.index(row, 0)
                        }

                        onDoubleClicked:
                            d.showSelectionOnLayout()
                    }

                    Item
                    {
                        // Single item which is re-parented to the hovered tile overlay.
                        id: tilePreviewOverlay

                        parent: eventGrid.tileController.hoveredTile
                            && eventGrid.tileController.hoveredTile.overlayContainer

                        anchors.fill: parent || undefined
                        visible: parent
                            && eventGrid.tileController.hoveredTile
                            && eventGrid.tileController.hoveredTile.hovered
                            && !showPreview

                        Column
                        {
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.margins: 4
                            spacing: 4

                            TileOverlayButton
                            {
                                icon.source: "image://skin/20x20/Solid/show_on_layout.svg"
                                accent: true

                                onClicked: eventModel.showOnLayout(
                                    eventGrid.tileController.hoveredTile.tileIndex)
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

                        view: showPreview ? null : eventGrid
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

                    model: eventModel

                    Shortcut
                    {
                        sequence: "Home"
                        enabled: eventGrid.count > 0 && showPreview

                        onActivated:
                            selection.index = eventModel.index(0, 0)
                    }

                    Shortcut
                    {
                        sequence: "End"
                        enabled: eventGrid.count > 0 && showPreview

                        onActivated:
                            selection.index = eventModel.index(eventModel.rowCount() - 1, 0)
                    }

                    Shortcut
                    {
                        sequence: "Left"
                        enabled: eventGrid.count > 0 && selection.index.valid && showPreview

                        onActivated:
                        {
                            const newRow = Math.max(selection.index.row - 1, 0)
                            selection.index = eventModel.index(newRow, 0)
                        }
                    }

                    Shortcut
                    {
                        sequence: "Right"
                        enabled: eventGrid.count > 0 && selection.index.valid && showPreview

                        onActivated:
                        {
                            const newRow = Math.min(selection.index.row + 1, eventModel.rowCount() - 1)
                            selection.index = eventModel.index(newRow, 0)
                        }
                    }

                    Shortcut
                    {
                        sequence: "Up"
                        enabled: eventGrid.count > 0 && selection.index.valid && showPreview

                        onActivated:
                        {
                            const newRow = selection.index.row -
                                (tileView ? eventGrid.numColumns : 1)
                            if (newRow >= 0)
                                selection.index = eventModel.index(newRow, 0)
                        }
                    }

                    Shortcut
                    {
                        sequence: "Down"
                        enabled: eventGrid.count > 0 && selection.index.valid && showPreview

                        onActivated:
                        {
                            const newRow = selection.index.row +
                                (tileView ? eventGrid.numColumns : 1)
                            if (newRow < eventModel.rowCount())
                                selection.index = eventModel.index(newRow, 0)
                        }
                    }

                    Shortcut
                    {
                        sequence: "PgUp"
                        enabled: eventGrid.count > 0 && selection.index.valid && showPreview

                        onActivated:
                        {
                            const rowsToSkip = tileView
                                ? eventGrid.rowsPerPage * eventGrid.numColumns
                                : tableView.rowsPerPage
                            const newRow = Math.max(0, selection.index.row - rowsToSkip)
                            selection.index = eventModel.index(newRow, 0)
                        }
                    }

                    Shortcut
                    {
                        sequence: "PgDown"
                        enabled: eventGrid.count > 0 && selection.index.valid && showPreview

                        onActivated:
                        {
                            const rowsToSkip = tileView
                                ? eventGrid.rowsPerPage * eventGrid.numColumns
                                : tableView.rowsPerPage
                            const newRow = Math.min(eventModel.rowCount() - 1,
                                selection.index.row + rowsToSkip)
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

                ScrollView
                {
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                    contentWidth: tableView.width - 8
                    clip: true

                    TableView
                    {
                        id: tableView

                        objectName: "AnalyticsSearchDialog.TableView"
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 16
                        horizontalHeaderVisible: true
                        readonly property int cellHeight: 28
                        readonly property int rowsPerPage:
                            Math.floor((height - topMargin - bottomMargin) / cellHeight)
                        property int hoveredRow: -1

                        clip: true
                        selectionBehavior: TableView.SelectRows
                        headerBackgroundColor: ColorTheme.colors.dark2

                        model: AnalyticsDialogTableModel
                        {
                            id: tableModel
                            sourceModel: eventModel
                            attributeManager: d.tableViewAttributeManager
                            objectTypeIds: analyticsFilters.selectedAnalyticsObjectTypeIds
                        }

                        Connections
                        {
                            target: selection
                            function onIndexChanged()
                            {
                                if (tableView.selectionModel.currentIndex.row === selection.index.row)
                                    return

                                tableView.selectionModel.setCurrentIndex(
                                    selection.index, ItemSelectionModel.Current)
                                tableView.positionViewAtRow(tableView.currentRow, TableView.Contain)
                            }
                        }

                        onCurrentRowChanged:
                        {
                            if (currentRow >= 0)
                                selection.index = eventModel.index(tableView.currentRow, 0)
                        }

                        delegate: Rectangle
                        {
                            id: tableDelegate

                            property bool isCurrentRow: tableView.currentRow === row
                            property bool isHoveredRow: tableView.hoveredRow === row
                            property var modelData: model

                            color: isCurrentRow && showPreview
                                ? ColorTheme.colors.dark9
                                : isHoveredRow ? ColorTheme.colors.dark4 : ColorTheme.colors.dark2
                            implicitWidth: Math.max(contentRowLayout.implicitWidth, 28)
                            implicitHeight: Math.max(
                                contentRowLayout.implicitHeight, tableView.cellHeight)

                            HoverHandler
                            {
                                onHoveredChanged:
                                {
                                    if (hovered)
                                        tableView.hoveredRow = row
                                    else if (tableView.hoveredRow === row)
                                        tableView.hoveredRow = -1
                                }
                            }

                            RowLayout
                            {
                                id: contentRowLayout

                                anchors.fill: parent
                                anchors.leftMargin: 8
                                spacing: 8

                                ColoredImage
                                {
                                    sourceSize: Qt.size(20, 20)
                                    sourcePath: model.iconKey
                                        ? "image://skin/" + model.iconKey
                                        : ""
                                    primaryColor: isHoveredRow
                                        ? ColorTheme.colors.light7
                                        : ColorTheme.colors.light10
                                    visible: !!sourcePath
                                }

                                Repeater
                                {
                                    model: tableDelegate.modelData.foregroundColor

                                    Rectangle
                                    {
                                        width: 16
                                        height: 16
                                        color: modelData ?? "transparent"
                                        border.color:
                                            ColorTheme.transparent(ColorTheme.colors.light1, 0.1)
                                        radius: 1
                                    }
                                }

                                Text
                                {
                                    Layout.fillWidth: true

                                    elide: Text.ElideRight
                                    font.pixelSize: 14

                                    color: isHoveredRow
                                        ? ColorTheme.colors.light7
                                        : ColorTheme.colors.light10
                                    text: model.display || ""
                                }

                                Item
                                {
                                    id: spacer
                                    Layout.fillWidth: true
                                }

                                ImageButton
                                {
                                    id: previewIcon

                                    rightPadding: 4

                                    icon.source: "image://skin/20x20/Solid/show_on_layout.svg"
                                    visible: isHoveredRow
                                        && column === tableView.model.columnCount() - 1

                                    onClicked:
                                        eventModel.showOnLayout(row)
                                }
                            }

                            MouseArea
                            {
                                anchors.fill: parent
                                enabled: !previewIcon.hovered
                                acceptedButtons: Qt.LeftButton | Qt.RightButton

                                onClicked: (mouse) =>
                                {
                                    if (mouse.button === Qt.RightButton)
                                    {
                                        eventModel.showContextMenu(
                                            row, parent.mapToGlobal(mouse.x, mouse.y), false)
                                        return
                                    }

                                    tableView.selectionModel.setCurrentIndex(
                                        tableModel.index(row, column), ItemSelectionModel.Current)
                                    if (!showPreview)
                                    {
                                        previewPanel.slideAnimationEnabled = true
                                        showPreview = true
                                    }
                                }

                                onDoubleClicked: (mouse) =>
                                {
                                    if (mouse.button === Qt.LeftButton)
                                        eventModel.showOnLayout(row)
                                }
                            }
                        }

                        RightPanel.ModelViewController
                        {
                            id: controller

                            view: tableView
                            model: eventModel
                            active: !tileView
                            loggingCategory: LoggingCategory { name: "Nx.RightPanel.TableView" }
                        }
                    }
                }

                RightPanelModel
                {
                    id: eventModel

                    context: windowContext
                    type: { return EventSearch.SearchType.analytics }
                    active: true
                    attributeManager: d.tileViewAttributeManager

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
            }
        }

        Resizer
        {
            id: previewPanelResizer

            z: 1
            edge: Qt.LeftEdge
            target: previewPanel
            anchors.rightMargin: -width //< Place the resizer insize previewPanel.

            enabled: showPreview

            onDragPositionChanged: (pos) =>
            {
                const requestedWidth =
                    previewPanel.x + previewPanel.width - pos - anchors.rightMargin
                previewPanel.setWidth(requestedWidth)
            }
        }

        PreviewPanel
        {
            id: previewPanel

            height: content.height
            width: 460
            x: showPreview ? content.width - width : content.width

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
                const attributes = accessor.getData(selection.index, "analyticsAttributes")
                const attribute = attributes[attributeRow]
                if (attribute)
                    header.searchText = createSearchRequestText(attribute.id, attribute.values)
            }

            onClose:
            {
                showPreview = false
            }

            selectedItem:
            {
                if (selection.index.row == -1 || !showPreview)
                    return null

                const getData = name => accessor.getData(selection.index, name)

                return {
                    "trackId": getData("trackId"),
                    "previewResource": getData("previewResource"),
                    "previewTimestampMs": getData("previewTimestampMs") || 0,
                    "previewAspectRatio": getData("previewAspectRatio") || 1.0,
                    "timestamp": getData("timestamp") || "",
                    "display": getData("display") || "",
                    "objectTitle": getData("objectTitle") || "",
                    "hasTitleImage": getData("hasTitleImage") || false,
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
            ? windowContext.systemContext.taxonomyManager.createFilterModel(dialog)
            : emptyFilterModel

        property AttributeDisplayManager tileViewAttributeManager:
            windowContext && eventModel.analyticsSetup
                ? AttributeDisplayManagerFactory.create(
                    AttributeDisplayManager.Mode.tileView, filterModel)
                : null

        property AttributeDisplayManager tableViewAttributeManager:
            windowContext && eventModel.analyticsSetup
                ? AttributeDisplayManagerFactory.create(
                    AttributeDisplayManager.Mode.tableView, filterModel)
                : null

        readonly property Analytics.Engine selectedAnalyticsEngine:
            tabBar.currentItem ? tabBar.currentItem.engine : null

        property var delayedAttributesFilter: []

        readonly property bool pluginTabsShown: filterModel.engines.length > 1

        readonly property bool objectTypeSelected:
            analyticsFilters.selectedAnalyticsObjectTypeIds.length > 0

        Analytics.FilterModel { id: emptyFilterModel }

        PropertyUpdateFilter on delayedAttributesFilter
        {
            source: analyticsFilters.selectedAttributeFilters
            minimumIntervalMs: LocalSettings.iniConfigValue("analyticsSearchRequestDelayMs")
        }

        function showSelectionOnLayout()
        {
            if (selection.index.valid)
                eventModel.showOnLayout(selection.index.row)
        }

        onSelectedAnalyticsEngineChanged:
        {
            if (!filterModel || filterModel.engines.length === 0)
                return

            storeCurrentEngineFilterState()

            if (!eventModel.analyticsSetup)
                return

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
                d.filterModel.selectedDevices = eventModel.commonSetup.selectedCameras
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

    FilterSettingsDialog
    {
        id: tileFilterSettingsDialog

        title: qsTr("Tile Settings")
        attributeManager: d.tileViewAttributeManager
        objectTypeIds: analyticsFilters.selectedAnalyticsObjectTypeIds
    }

    FilterSettingsDialog
    {
        id: tableFilterSettingsDialog

        title: qsTr("Table Settings")
        attributeManager: d.tableViewAttributeManager
        objectTypeIds: analyticsFilters.selectedAnalyticsObjectTypeIds
    }
}
