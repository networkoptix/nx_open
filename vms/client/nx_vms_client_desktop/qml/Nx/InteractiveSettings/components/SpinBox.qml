// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6

import Nx.Controls 1.0

import nx.vms.client.desktop 1.0

import "private"

LabeledItem
{
    id: control

    property int defaultValue: 0
    property alias minValue: spinBox.from
    property alias maxValue: spinBox.to
    property alias value: spinBox.value

    readonly property bool filled: spinBox.value !== 0

    defaultValueTooltipEnabled: true

    contentItem: Item
    {
        implicitWidth: spinBox.implicitWidth
        implicitHeight: spinBox.implicitHeight
        baselineOffset: spinBox.baselineOffset

        SpinBox
        {
            id: spinBox

            editable: true
            value: defaultValue
            GlobalToolTip.text: control.defaultValueTooltipText(defaultValue)
        }
    }
}
