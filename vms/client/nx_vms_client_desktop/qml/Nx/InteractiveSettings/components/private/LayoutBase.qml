// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

Control
{
    id: layout

    property list<Item> layoutItems

    signal itemAdded(Item item)

    onChildrenChanged:
    {
        for (let item of Array.from(children).filter(item => (item !== contentItem)))
        {
            layoutItems.push(item)
            itemAdded(item)
        }
    }
}
