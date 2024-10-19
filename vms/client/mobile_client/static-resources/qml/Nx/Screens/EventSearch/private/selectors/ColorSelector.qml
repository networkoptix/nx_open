// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import Nx.Core 1.0
import Nx.Mobile.Controls 1.0

import nx.vms.client.core.analytics

import "../editors"

/**
 * General color selection selector.
 */
OptionSelector
{
    id: control

    property Attribute attribute: null

    visualDelegate: Rectangle
    {
        property var data
        readonly property var colorValue: data || control.value

        width: !!data
            ? 32
            : 20
        height: width
        visible: !!colorValue
        radius: 4
        color: control.attribute.colorSet.color(colorValue)
    }

    screenDelegate: GridEditor
    {
        model: CoreUtils.toArray(attribute && attribute.colorSet && attribute.colorSet.items)
        visualDelegate: control.visualDelegate
    }
}
