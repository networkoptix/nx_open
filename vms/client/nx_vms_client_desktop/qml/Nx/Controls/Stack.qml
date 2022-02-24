// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14

Item
{
    id: stack

    property int currentIndex: -1

    readonly property int count: children.length
    readonly property Item currentItem: children[currentIndex] ?? null

    implicitWidth: childrenRect.x + childrenRect.width
    implicitHeight: childrenRect.y + childrenRect.height

    onChildrenChanged:
    {
        for (let i = 0; i < children.length; ++i)
            children[i].visible = Qt.binding(() => i === currentIndex)
    }
}
