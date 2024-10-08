// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts

import Nx.Controls
import Nx.RightPanel

import nx.vms.client.desktop

import ".."

SearchPanel
{
    id: eventsPanel

    type: { return RightPanelModel.Type.events }

    placeholder
    {
        icon: "image://skin/64x64/Outline/events.svg"
        title: qsTr("No events")
        description: qsTr("Try changing the filters or create an Event Rule")
    }

    EventTypeSelector
    {
        id: eventTypeSelector

        model: eventsPanel.model
        parent: eventsPanel.filtersColumn
        Layout.maximumWidth: eventsPanel.filtersColumn.width
    }

    onFiltersReset:
        eventTypeSelector.deactivate()
}
