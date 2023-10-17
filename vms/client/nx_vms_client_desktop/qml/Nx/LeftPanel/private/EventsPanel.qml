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

import "analytics"

import "metrics.js" as Metrics

Item
{
    id: eventsPanel

    property alias active: eventModel.active
    property alias model: eventModel
    property int defaultCameraSelection: { return RightPanel.CameraSelection.all }

    property alias showThumbnails: counterBlock.showThumbnails
    property alias showInformation: counterBlock.showInformation

    property bool resizeEnabled: true

    property real desiredFiltersWidth: Metrics.kDefaultFilterColumnWidth
    property real maximumWidth: Number.MAX_VALUE

    readonly property real maximumFiltersWidth:
        Math.min(Metrics.kMaximumFilterColumnWidth, eventsPanel.maximumWidth - resultsColumn.width)

    readonly property real filtersWidth: MathUtils.bound(
        Metrics.kMinimumFilterColumnWidth,
        eventsPanel.desiredFiltersWidth,
        eventsPanel.maximumFiltersWidth)

    implicitWidth: eventsPanel.filtersWidth + resultsColumn.width

    signal filtersReset()

    Scrollable
    {
        id: filtersPanel

        width: eventsPanel.filtersWidth - scrollBar.width
        height: eventsPanel.height

        verticalScrollBar: scrollBar

        contentItem: SearchPanelHeader
        {
            id: header

            model: eventModel

            width: filtersPanel.width
            height: implicitHeight

            GenericTypeSelector
            {
                id: typeSelector

                name: qsTr("Category")
                width: header.width
                model: eventModel.eventCategories
                idRole: "type"
                iconPath: (iconName => `image://svg/skin/left_panel/events/${iconName}`)

                onSelectedTypeIdChanged:
                {
                    if (selectedTypeId !== EventCategory.Analytics)
                        subtypeSelector.selectedValue = undefined
                }
            }

            AnalyticsEventSelector
            {
                id: subtypeSelector

                topPadding: 8
                width: header.width
                model: eventModel
                visible: count && typeSelector.selectedTypeId === EventCategory.Analytics
            }

            Binding
            {
                target: eventModel.sourceModel
                property: "selectedEventType"
                value: typeSelector.selectedTypeId ?? EventCategory.Any
            }

            Binding
            {
                target: eventModel.sourceModel
                property: "selectedSubType"
                value: subtypeSelector.selectedValue ?? ""
            }

            Text
            {
                id: seeEventLogHint

                topPadding: 24
                width: header.width
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: 12
                color: ColorTheme.colors.dark11

                // Text.StyledText hyperlink coordinates computation is bugged in QtQuick.
                textFormat: Text.RichText

                // This property is used intrinsically with Text.StyledText format.
                // We reuse it but explicitly inject its value to rich text as a CSS attribute.
                linkColor: hoveredLink ? ColorTheme.colors.dark12 : ColorTheme.colors.dark11

                text:
                {
                    const eventLog = qsTr("Event Log")

                    return qsTr("See %1 for other events").arg(
                        `<a href="dummy" style="color:${linkColor};">${eventLog}</a><br>`)
                }

                CursorOverride.active: !!hoveredLink
                CursorOverride.shape: Qt.PointingHandCursor

                onLinkActivated:
                    eventModel.showEventLog()
            }

            onFiltersReset:
            {
                eventModel.commonSetup.cameraSelection = eventsPanel.defaultCameraSelection
            }
        }
    }

    ScrollBar
    {
        id: scrollBar

        height: eventsPanel.height
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
        enabled: eventsPanel.resizeEnabled
        handleWidth: Metrics.kResizerWidth
        drag.minimumX: Metrics.kMinimumFilterColumnWidth
        drag.maximumX: eventsPanel.maximumFiltersWidth

        anchors.leftMargin: scrollBar.width

        onDragPositionChanged:
            eventsPanel.desiredFiltersWidth = pos
    }

    Column
    {
        id: resultsColumn

        anchors.left: scrollBar.right

        width: Metrics.kFixedSearchPanelWidth
        height: eventsPanel.height

        CounterBlock
        {
            id: counterBlock

            width: resultsColumn.width
            displayedItemsText: eventModel.itemCountText
        }

        EventRibbon
        {
            id: eventRibbon

            objectName: eventsPanel.objectName + ".EventRibbon"

            placeholder
            {
                icon: "image://svg/skin/left_panel/placeholders/events.svg"
                title: qsTr("No events")
                description: qsTr("Try changing the filters or create an Event Rule")
            }

            tileController
            {
                showThumbnails: eventsPanel.showThumbnails
                showInformation: eventsPanel.showInformation
                showIcons: false
            }

            width: resultsColumn.width

            height: eventsPanel.height - counterBlock.height

            model: RightPanelModel
            {
                id: eventModel

                context: windowContext
                type: { return RightPanelModel.Type.events }
                previewsEnabled: eventsPanel.showThumbnails

                Component.onCompleted:
                    commonSetup.cameraSelection = eventsPanel.defaultCameraSelection
            }
        }
    }
}
