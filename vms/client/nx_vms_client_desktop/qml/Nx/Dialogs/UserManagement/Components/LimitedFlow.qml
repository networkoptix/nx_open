// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import nx.vms.client.desktop

Flow
{
    id: control

    spacing: 8

    property int rowLimit: 3
    property alias sourceModel: limitedModel.sourceModel

    property alias delegate: groupsRepeater.delegate
    readonly property int remaining: limitedModel.remaining

    readonly property real maxHeight:
        rowLimit * (moreGroupsButton.height + control.spacing) - control.spacing
        + topPadding
        + bottomPadding

    default property alias data: moreGroupsButton.data

    readonly property real widthAdjustment: leftPadding + rightPadding
        + (rowLimit == 1 && remaining > 0
            ? (moreGroupsButton.childrenRect.width + spacing)
            : 0)
    readonly property real availableWidth: width - widthAdjustment

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

        const rowHeight = moreGroupsButton.height + (control.rowLimit > 1 ? control.spacing : 0)
        const errorMargin = control.spacing * 0.5
        const yLimit = control.topPadding + control.rowLimit * rowHeight - errorMargin

        const childrenWithinLimit = control.childrenRect.height < yLimit

        if (childrenWithinLimit)
        {
            ++limitedModel.limit
            return
        }

        groupsRepeater.repositioning = false
        limitedModel.limit = Math.max(1, limitedModel.limit - 1)
    }
}
