// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0

import Nx.Controls 1.0

import nx.vms.client.desktop 1.0

import "private"

LabeledItem
{
    id: control

    property real defaultValue: 0
    property alias minValue: spinBox.dFrom
    property alias maxValue: spinBox.dTo
    property alias value: spinBox.dValue
    property alias decimals: spinBox.decimals
    property alias step: spinBox.dStepSize

    defaultValueTooltipEnabled: true

    contentItem: Item
    {
        implicitWidth: spinBox.implicitWidth
        implicitHeight: spinBox.implicitHeight
        baselineOffset: spinBox.baselineOffset

        DoubleSpinBox
        {
            id: spinBox

            editable: true
            dValue: defaultValue
            GlobalToolTip.text: control.defaultValueTooltipText(defaultValue)
        }
    }
}
