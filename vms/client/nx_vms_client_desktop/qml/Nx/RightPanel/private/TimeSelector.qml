// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Controls

import nx.vms.client.core
import nx.vms.client.desktop

SelectableTextButton
{
    id: timeSelector

    property CommonObjectSearchSetup setup

    selectable: false
    accented: setup && setup.timeSelection === EventSearch.TimeSelection.selection
    icon.source: "image://skin/20x20/Outline/calendar.svg"
    text: setup ? actionNames[setup.timeSelection] : ""

    readonly property var actionNames:
    {
        let result = {}
        result[EventSearch.TimeSelection.anytime] = qsTr("Any time")
        result[EventSearch.TimeSelection.day] = qsTr("Last day")
        result[EventSearch.TimeSelection.week] = qsTr("Last 7 days")
        result[EventSearch.TimeSelection.month] = qsTr("Last 30 days")
        result[EventSearch.TimeSelection.selection] = qsTr("Selected on Timeline")
        return result
    }

    function updateState()
    {
        timeSelector.setState(
            !setup || setup.timeSelection === EventSearch.TimeSelection.anytime
                ? SelectableTextButton.State.Deactivated
                : SelectableTextButton.State.Unselected)
    }

    onStateChanged:
    {
        if (state === SelectableTextButton.State.Deactivated)
            defaultTimeSelection.trigger()
    }

    Component.onCompleted:
        updateState()

    Connections
    {
        target: setup

        function onTimeSelectionChanged()
        {
            timeSelector.updateState()
        }
    }

    menu: CompactMenu
    {
        id: menuControl

        component MenuAction: Action { text: timeSelector.actionNames[data] }

        MenuAction { data: EventSearch.TimeSelection.day }
        MenuAction { data: EventSearch.TimeSelection.week }
        MenuAction { data: EventSearch.TimeSelection.month }
        CompactMenuSeparator {}
        MenuAction { id: defaultTimeSelection; data: EventSearch.TimeSelection.anytime }

        onTriggered: (action) =>
        {
            if (setup)
                setup.timeSelection = action.data
        }
    }
}
