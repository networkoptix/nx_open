// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx
import Nx.Controls

import nx.vms.client.core.analytics
import nx.vms.client.desktop
import nx.vms.client.desktop.analytics

import "metrics.js" as Metrics

SelectableTextButton
{
    id: objectTypeSelector

    property AnalyticsSearchSetup setup
    property ObjectTypeModel model

    readonly property AnalyticsFilterModel filterModel: model ? model.sourceModel : null

    readonly property ObjectType selectedObjectType: setup && filterModel
        ? filterModel.findFilterObjectType(setup.objectTypes)
        : null

    selectable: false

    text:
    {
        if (!selectedObjectType)
            return anyTypeAction.text

        const numFilters = setup.attributeFilters.length
        const suffix = numFilters ? (" " + qsTr("with %n attributes", "", numFilters)) : ""
        return selectedObjectType.name + suffix
    }

    icon.source: IconManager.iconUrl(selectedObjectType ? selectedObjectType.iconSource : "")

    visible: objectTypesRepeater.count > 0

    function updateState()
    {
        objectTypeSelector.setState(
            !setup || setup.objectTypes.length === 0
                ? SelectableTextButton.State.Deactivated
                : SelectableTextButton.State.Unselected)
    }

    onStateChanged:
    {
        if (state === SelectableTextButton.State.Deactivated)
            anyTypeAction.trigger()
    }

    Component.onCompleted:
        updateState()

    Connections
    {
        target: setup

        function onObjectTypesChanged()
        {
            objectTypeSelector.updateState()
        }
    }

    menu: CompactMenu
    {
        id: menuControl

        Action
        {
            id: anyTypeAction
            text: qsTr("Any type")
            data: []
        }

        CompactMenuSeparator {}

        Repeater
        {
            id: objectTypesRepeater

            model: LinearizationListModel
            {
                sourceModel: objectTypeSelector.model
                autoExpandAll: true
            }

            CompactMenuItem
            {
                leftPadding: 8 + Metrics.kTreeMenuIndentation * model.level

                action: Action
                {
                    text: model.name
                    data: model.ids
                }
            }
        }

        onTriggered: (action) =>
        {
            if (!setup)
                return

            setup.objectTypes = action.data
            setup.attributeFilters = []
        }
    }
}
