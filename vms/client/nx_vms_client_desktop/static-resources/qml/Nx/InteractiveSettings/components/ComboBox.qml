import QtQuick 2.0

import Nx.Controls 1.0

import "private"

LabeledItem
{
    id: control

    property var defaultTooltip: (itemCaptions && itemCaptions[defaultValue]) || defaultValue
    property var range
    property var value: defaultValue
    property var defaultValue
    property var itemCaptions

    onValueChanged: comboBox.currentIndex = range.indexOf(value)

    contentItem: ComboBox
    {
        id: comboBox
        onActivated: control.value = range[currentIndex]

        model:
        {
            if (!Array.isArray(range))
                return null

            return range.map(
                function (name) { return (itemCaptions && itemCaptions[name]) || name })
        }
    }
}
