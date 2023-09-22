// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0

import nx.vms.client.core 1.0

SpinBox
{
    id: control

    property real dFrom: 0.0
    property real dTo: 100.0
    property real dValue: 0.0
    property int decimals: 3
    property real dStepSize: 1.0 / multiplier
    property real dPageSize: dStepSize * 10.0

    readonly property int multiplier: Math.pow(10, decimals)

    from: dFrom * multiplier
    to: dTo * multiplier
    stepSize: dStepSize * multiplier
    pageSize: dPageSize * multiplier
    inputMethodHints: Qt.ImhFormattedNumbersOnly

    validator: DoubleValidator
    {
        top: dTo
        bottom: dFrom
        locale: control.locale
        decimals: control.decimals
        notation: DoubleValidator.StandardNotation
    }

    onDValueChanged:
        value = Math.round(dValue * multiplier)

    onValueChanged:
        dValue = value / multiplier

    textFromValue: (function(value, locale)
    {
        return decoratedText(Number(value / multiplier).toLocaleString(locale, "f", decimals))
    })

    valueFromText: (function(text, locale)
    {
        const value = Number.fromLocaleString(locale, strippedText(text))
        return Math.round(value ? value * multiplier : 0.0)
    })
}
