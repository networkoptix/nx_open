// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14

import Nx 1.0
import Nx.Core 1.0
import Nx.Controls 1.0

import nx.vms.client.desktop.analytics 1.0 as Analytics

ComboBox
{
    id: control

    property Analytics.Attribute attribute

    property var selectedValue

    editable: true
    currentIndex: -1
    model: Utils.toArray(attribute && attribute.enumeration && attribute.enumeration.items)

    onSelectedValueChanged:
    {
        editText = Utils.getValue(selectedValue, "")
        currentIndex = find(editText)
    }

    function setSelected(value)
    {
        selectedValue = (value && find(value) !== -1) ? value : undefined
        focus = false
    }

    Keys.onEscapePressed: setSelected(selectedValue)

    onActivated: setSelected(editText)
    onAccepted: setSelected(editText)
    onActiveFocusChanged:
    {
        if (!activeFocus)
            setSelected(editText)
    }
}
