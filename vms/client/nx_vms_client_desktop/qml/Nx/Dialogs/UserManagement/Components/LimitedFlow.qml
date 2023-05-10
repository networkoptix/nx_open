// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import nx.vms.client.desktop

Flow
{
    id: control

    spacing: 8

    property int rowLimit: 4
    property alias sourceModel: limitedModel.sourceModel

    property alias delegate: groupsRepeater.delegate
    readonly property int remaining: limitedModel.remaining

    readonly property real maxHeight:
        rowLimit * (moreGroupsButton.height + control.spacing) - control.spacing
        + topPadding
        + bottomPadding

    default property alias data: moreGroupsButton.data

    Repeater
    {
        id: groupsRepeater

        property bool repositioning: true

        model: LimitedModel
        {
            id: limitedModel
            limit: 0
            onSourceCountChanged: repopulate()
        }
    }

    Item
    {
        id: moreGroupsButton

        width: childrenRect.width
        height: childrenRect.height

        visible: limitedModel.remaining > 0
    }

    onWidthChanged: repopulate()

    function repopulate()
    {
        limitedModel.limit = 1
        groupsRepeater.repositioning = true
        forceLayout()
    }

    onPositioningComplete:
    {
        if (!groupsRepeater.repositioning)
            return

        const rowHeight = moreGroupsButton.height + control.spacing
        const errorMargin = control.spacing * 0.5
        const yLimit = control.topPadding + control.rowLimit * rowHeight - errorMargin

        const childrenWithinLimit = control.childrenRect.height < yLimit

        if (childrenWithinLimit)
        {
            ++limitedModel.limit
            return
        }

        groupsRepeater.repositioning = false
        --limitedModel.limit
    }
}
