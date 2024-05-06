// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Controls

import nx.vms.client.desktop

SelectableTextButton
{
    id: timeSelector

    property CommonObjectSearchSetup setup

    selectable: false
    accented: setup && setup.timeSelection === RightPanel.TimeSelection.selection
    icon.source: "image://skin/20x20/Outline/calendar.svg"
    text: setup ? actionNames[setup.timeSelection] : ""

    readonly property var actionNames:
    {
        let result = {}
        result[RightPanel.TimeSelection.anytime] = qsTr("Any time")
        result[RightPanel.TimeSelection.day] = qsTr("Last day")
        result[RightPanel.TimeSelection.week] = qsTr("Last 7 days")
        result[RightPanel.TimeSelection.month] = qsTr("Last 30 days")
        result[RightPanel.TimeSelection.selection] = qsTr("Selected on Timeline")
        return result
    }

    function updateState()
    {
        timeSelector.setState(
            !setup || setup.timeSelection === RightPanel.TimeSelection.anytime
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

        MenuAction { data: RightPanel.TimeSelection.day }
        MenuAction { data: RightPanel.TimeSelection.week }
        MenuAction { data: RightPanel.TimeSelection.month }
        CompactMenuSeparator {}
        MenuAction { id: defaultTimeSelection; data: RightPanel.TimeSelection.anytime }

        onTriggered: (action) =>
        {
            if (setup)
                setup.timeSelection = action.data
        }
    }
}
