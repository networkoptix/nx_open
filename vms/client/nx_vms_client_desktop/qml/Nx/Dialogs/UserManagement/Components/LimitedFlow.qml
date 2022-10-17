// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

Flow
{
    id: control

    spacing: 8

    property int rowLimit: 2
    property var items: []
    property alias delegate: groupsRepeater.delegate
    readonly property int remaining: moreGroupsButton.remaining
    default property alias data: moreGroupsButton.data

    visible: items.length > 0

    onItemsChanged: repopulate()

    Repeater
    {
        id: groupsRepeater
        model: []
    }

    Item
    {
        id: moreGroupsButton

        width: childrenRect.width
        height: childrenRect.height

        property int remaining: 0
        visible: control.remaining > 0
    }

    onWidthChanged: repopulate()

    function repopulate()
    {
        groupsRepeater.model = items.slice()
        moreGroupsButton.remaining = items ? items.length : 0
    }

    onPositioningComplete:
    {
        const rowHeight = moreGroupsButton.height + groupsFlow.spacing
        const yLimit = groupsFlow.rowLimit * rowHeight
        const errorMargin = groupsFlow.spacing * 0.5
        const firstOnFirstInvisibleRow = moreGroupsButton.x < errorMargin
            && moreGroupsButton.y < (yLimit + errorMargin)
        const allAreVisible = (items.length - groupsRepeater.model.length) == 0

        if (moreGroupsButton.y >= yLimit - errorMargin
            && !(firstOnFirstInvisibleRow && allAreVisible))
        {
            // Assinging a new array (with the last element removed) should cause repositioning.
            groupsRepeater.model = groupsRepeater.model.slice(0, -1)
        }

        moreGroupsButton.remaining = items.length - groupsRepeater.model.length
    }
}
