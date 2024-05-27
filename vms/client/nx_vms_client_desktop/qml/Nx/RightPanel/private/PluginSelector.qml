// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx
import Nx.Controls

import nx.vms.client.desktop
import nx.vms.client.desktop.analytics

SelectableTextButton
{
    id: pluginSelector

    property AnalyticsSearchSetup setup
    property AnalyticsFilterModel model

    selectable: false

    icon.source: "image://skin/20x20/Outline/plugins.svg"

    text:
    {
        if (!setup || !model)
            return ""

        if (setup.engine.isNull())
            return anyPluginAction.text

        const engine = model.findEngine(setup.engine)
        return engine ? engine.name : ""
    }

    function updateState()
    {
        pluginSelector.setState(
            !setup || setup.engine.isNull()
                ? SelectableTextButton.State.Deactivated
                : SelectableTextButton.State.Unselected)
    }

    onStateChanged:
    {
        if (state === SelectableTextButton.State.Deactivated)
            anyPluginAction.trigger()
    }

    Component.onCompleted:
        updateState()

    Connections
    {
        target: setup

        function onEngineChanged()
        {
            pluginSelector.updateState()
        }
    }

    menu: CompactMenu
    {
        id: menuControl

        CompactMenuItem
        {
            action: Action
            {
                id: anyPluginAction
                text: qsTr("Any plugin")
                data: ""
            }
        }

        CompactMenuSeparator {}

        Repeater
        {
            model: pluginSelector.model ? pluginSelector.model.engines : []

            CompactMenuItem
            {
                action: Action
                {
                    text: modelData.name
                    data: modelData.id
                }
            }
        }

        onTriggered: (action) =>
        {
            if (setup)
                setup.engine = NxGlobals.uuid(action.data)
        }
    }
}
