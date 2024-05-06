// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx
import Nx.Controls

import nx.vms.client.desktop

import "metrics.js" as Metrics

SelectableTextButton
{
    id: eventTypeSelector

    property RightPanelModel model
    readonly property var eventSearchModel: model ? model.sourceModel : null

    readonly property var eventGroups:
    {
        let result = {}
        const groups = model ? model.eventGroups : []

        for (let group of groups)
            result[group.type] = group

        return result
    }

    selectable: false

    icon.source: "image://skin/text_buttons/event_rules_20x20.svg"

    text:
    {
        if (!eventSearchModel)
            return ""

        for (let key in eventGroups)
        {
            if (eventSearchModel.selectedEventType === eventGroups[key].id)
            {
                if (key != VmsEventGroup.Analytics)
                    return eventGroups[key].any

                if (!eventSearchModel.selectedSubType)
                {
                    return dynamicItems.count
                        ? eventGroups[key].any
                        : analyticsEventAction.text
                }

                const index = findSelectedSubTypeIndex()
                return NxGlobals.modelData(index, "name")
            }

            const event = Array.prototype.find.call(eventGroups[key].events,
                item => item.id === eventSearchModel.selectedEventType)

            if (event)
                return event.name
        }

        return ""
    }

    function updateState()
    {
        eventTypeSelector.setState(eventSearchModel
            && eventSearchModel.selectedEventType !== eventGroups[VmsEventGroup.Common].id
                ? SelectableTextButton.State.Unselected
                : SelectableTextButton.State.Deactivated)
    }

    onStateChanged:
    {
        if (state === SelectableTextButton.State.Deactivated)
            anyEventAction.trigger()
    }

    Component.onCompleted:
        updateState()

    Connections
    {
        target: eventSearchModel

        function onSelectedEventTypeChanged()
        {
            eventTypeSelector.updateState()
        }
    }

    Connections
    {
        target: eventTypeSelector.model.analyticsEvents

        function onChanged()
        {
            if (!eventSearchModel.selectedSubType)
                return

            if (!findSelectedSubTypeIndex().valid)
                eventSearchModel.selectedSubType = ""
        }
    }

    function findSelectedSubTypeIndex()
    {
        if (!eventSearchModel || !model || model.analyticsEvents.rowCount() === 0)
            return NxGlobals.invalidModelIndex()

        return NxGlobals.modelFindOne(
            model.analyticsEvents.index(0, 0, NxGlobals.invalidModelIndex()),
            "id",
            eventSearchModel.selectedSubType,
            Qt.MatchExactly | Qt.MatchRecursive)
    }

    menu: CompactMenu
    {
        Action
        {
            id: anyEventAction
            text: eventGroups[VmsEventGroup.Common].any
            data: 0
        }

        CompactMenuSeparator {}

        Action
        {
            id: analyticsEventAction

            text: qsTr("Analytics Event")
            data: eventGroups[VmsEventGroup.Analytics].id
            visible: dynamicItems.count === 0
        }

        CompactMenu
        {
            id: analyticsSubMenu

            title: eventGroups[VmsEventGroup.Analytics].name
            notifiesParentMenu: false

            Action
            {
                id: anyAnalyticsEventAction
                text: eventGroups[VmsEventGroup.Analytics].any
                data: ""
            }

            CompactMenuSeparator {}

            DynamicMenuItems
            {
                id: dynamicItems

                parentMenu: analyticsSubMenu
                separator: CompactMenuSeparator {}
                model: eventTypeSelector.model.analyticsEvents
                textRole: "name"
                dataRole: "id"
            }

            Binding
            {
                target: analyticsSubMenu.parent
                property: "visible"
                value: dynamicItems.count > 0
            }

            onTriggered: (action) =>
            {
                if (!eventSearchModel)
                    return

                eventSearchModel.selectedEventType = eventGroups[VmsEventGroup.Analytics].id
                eventSearchModel.selectedSubType = action.data
            }
        }

        Repeater
        {
            model: eventGroups[VmsEventGroup.Common].events

            CompactMenuItem
            {
                action: Action
                {
                    text: modelData.name
                    data: modelData.id
                }
            }
        }

        CompactMenuSeparator {}

        CompactMenu
        {
            title: eventGroups[VmsEventGroup.DeviceIssues].name

            Action
            {
                text: eventGroups[VmsEventGroup.DeviceIssues].any
                data: eventGroups[VmsEventGroup.DeviceIssues].id
            }

            CompactMenuSeparator {}

            Repeater
            {
                model: eventGroups[VmsEventGroup.DeviceIssues].events

                CompactMenuItem
                {
                    action: Action
                    {
                        text: modelData.name
                        data: modelData.id
                    }
                }
            }
        }

        CompactMenu
        {
            title: eventGroups[VmsEventGroup.Server].name

            Action
            {
                text: eventGroups[VmsEventGroup.Server].any
                data: eventGroups[VmsEventGroup.Server].id
            }

            CompactMenuSeparator {}

            Repeater
            {
                model: eventGroups[VmsEventGroup.Server].events

                CompactMenuItem
                {
                    action: Action
                    {
                        text: modelData.name
                        enabled: !modelData.hasChildren
                        data: modelData.id
                    }
                }
            }
        }

        onTriggered: (action) =>
        {
            if (!eventSearchModel)
                return

            eventSearchModel.selectedEventType = action.data
            eventSearchModel.selectedSubType = ""
        }
    }
}
