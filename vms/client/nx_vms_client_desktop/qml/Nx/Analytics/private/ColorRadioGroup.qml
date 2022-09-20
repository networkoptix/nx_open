// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14

import Nx 1.0

import nx.vms.client.desktop.analytics 1.0 as Analytics

RadioGroup
{
    id: control

    property Analytics.Attribute attribute

    model: Utils.toArray(attribute && attribute.colorSet && attribute.colorSet.items)

    middleItemDelegate: Rectangle
    {
        width: 16
        height: 16
        color: attribute.colorSet.color(modelData)
        border.color: ColorTheme.transparent(ColorTheme.colors.light1, 0.1)
        radius: 1
    }
}
