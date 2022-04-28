// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.11

import Nx.Controls 1.0

import nx.vms.client.desktop 1.0

import "private"

LabeledItem
{
    id: control

    isGroup: buttons.count > 1

    property var value: defaultValue
    property var defaultValue
    property var itemCaptions
    property alias range: buttons.model
    property bool isActive: false

    readonly property bool filled: value

    contentItem: MultiColumn
    {
        id: buttons

        baselineOffset: (count > 0) ? itemAt(0).baselineOffset : 0

        delegate: RadioButton
        {
            readonly property var identifier: modelData

            text: (control.itemCaptions && control.itemCaptions[identifier]) || identifier
            checked: control.value === identifier

            onClicked:
            {
                if (control.value !== identifier)
                    control.value = identifier
            }

            GlobalToolTip.text: control.defaultValueTooltipText(
                (itemCaptions && itemCaptions[defaultValue]) || defaultValue)
        }
    }
}
