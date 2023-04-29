// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Controls

import nx.vms.client.desktop

Row
{
    id: control

    property int seconds: 0
    property bool enabled: true

    QtObject
    {
        id: privateImpl

        property bool blockUpdates: false
    }

    onSecondsChanged:
    {
        if (privateImpl.blockUpdates)
            return

        const state = getState(seconds)

        timeSpinBox.value = state.value
        suffixComboBox.currentIndex = state.index
    }

    function getState(seconds)
    {
        // Select the best suitable suffix.
        for (let i = TimeDurationSuffixModel.Suffix.count - 1; i > 0; --i)
        {
            const mult = suffixModel.multiplierAt(i)
            const value = Math.floor(seconds / mult)

            if (value != 0 && seconds == value * mult)
                return {"value": value, "index": i}
        }

        return {"value": seconds, "index": 0}
    }

    function updateSeconds()
    {
        privateImpl.blockUpdates = true

        control.seconds = timeSpinBox.value * suffixModel.multiplierAt(suffixComboBox.currentIndex)

        privateImpl.blockUpdates = false
    }

    spacing: 8

    TimeDurationSuffixModel
    {
        id: suffixModel
        value: timeSpinBox.value
    }

    SpinBox
    {
        id: timeSpinBox

        editable: true
        from: 0
        to: 9999999

        enabled: control.enabled
        onValueChanged: updateSeconds()
    }

    ComboBox
    {
        id: suffixComboBox

        enabled: control.enabled

        width: Math.max(90, requiredContentWidth)
        model: suffixModel
        textRole: "display"

        onCurrentIndexChanged: updateSeconds()
    }
}
