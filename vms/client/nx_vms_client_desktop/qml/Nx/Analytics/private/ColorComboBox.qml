// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14

import Nx.Controls 1.0
import Nx.Core 1.0

import nx.vms.client.core.analytics 1.0 as Analytics

ComboBox
{
    id: control

    property Analytics.Attribute attribute

    property var selectedValue

    displayText: CoreUtils.getValue(selectedValue, "")
    displayedColor: selectedValue !== undefined ? currentValue : "transparent"

    model:
    {
        if (!attribute || !attribute.colorSet)
            return []

        return Array.prototype.map.call(attribute.colorSet.items,
            name => ({"name": name, "color": attribute.colorSet.color(name)}))
    }

    withColorSection: true
    textRole: "name"
    valueRole: "color"

    onActivated:
        selectedValue = currentText
}
