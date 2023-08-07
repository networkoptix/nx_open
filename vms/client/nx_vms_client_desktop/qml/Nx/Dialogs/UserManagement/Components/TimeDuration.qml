// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Controls

import nx.vms.client.desktop

Row
{
    id: control

    property int seconds: 0
    property bool enabled: true

    property alias valueFrom: timeSpinBox.from
    property alias valueTo: timeSpinBox.to

    property alias suffixes: suffixModel.suffixes
    onSuffixesChanged: updateSuffix()

    QtObject
    {
        id: privateImpl

        property bool blockUpdates: false
    }

    onSecondsChanged:
    {
        if (privateImpl.blockUpdates)
            return

        updateSuffix()
    }

    function getState(seconds)
    {
        // Select the best suitable suffix.
        for (let i = suffixModel.suffixes.length - 1; i > 0; --i)
        {
            const mult = suffixModel.multiplierAt(i)
            const value = Math.floor(seconds / mult)

            if (value != 0 && seconds == value * mult)
                return {"value": value, "index": i}
        }

        return {"value": seconds, "index": 0}
    }

    function updateSuffix()
    {
        const state = getState(seconds)

        timeSpinBox.value = state.value
        suffixComboBox.currentIndex = state.index
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
