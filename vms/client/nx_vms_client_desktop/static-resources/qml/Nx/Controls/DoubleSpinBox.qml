import QtQuick 2.0

import nx.vms.client.core 1.0

SpinBox
{
    id: control

    property real dFrom: -Infinity
    property real dTo: Infinity
    property real dValue: 0.0
    property int decimals: 3
    property real dStepSize: 1.0 / multiplier

    readonly property int multiplier: Math.pow(10, decimals)

    from: dFrom * multiplier
    to: dTo * multiplier
    stepSize: dStepSize * multiplier
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
        return Number(value / multiplier).toLocaleString(locale, "f", decimals)
    })

    valueFromText: (function(text, locale)
    {
        return Math.round(Number.fromLocaleString(locale, text) * multiplier)
    })
}
