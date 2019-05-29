import QtQuick 2.0
import Nx.Controls 1.0

import "private"

LabeledItem
{
    id: control

    property real defaultValue: 0
    property real minValue: -Infinity
    property real maxValue: Infinity
    property real value: defaultValue
    property int decimals: 3
    property real step: 1 / spinBox.multiplier

    onValueChanged: spinBox.value = value * spinBox.multiplier

    contentItem: SpinBox
    {
        id: spinBox

        readonly property int multiplier: Math.pow(10, decimals)

        from: minValue * multiplier
        to: maxValue * multiplier
        stepSize: step * multiplier

        editable: true
        validator: DoubleValidator { bottom: spinBox.from; top: spinBox.to }

        onValueChanged: control.value = value / multiplier

        textFromValue: function(value, locale)
        {
            return Number(value / multiplier).toLocaleString(locale, "f", decimals)
        }

        valueFromText: function(text, locale)
        {
            return Number.fromLocaleString(locale, text) * multiplier
        }
    }
}

