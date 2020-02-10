import QtQuick 2.11

import Nx.Controls 1.0

import "private"

LabeledItem
{
    id: control

    isGroup: buttons.count > 1

    property var value: defaultValue
    property var defaultValue
    property alias range: buttons.model

    contentItem: MultiColumn
    {
        id: buttons

        baselineOffset: (count > 0) ? itemAt(0).baselineOffset : 0

        delegate: CheckBox
        {
            text: modelData

            checked: Array.isArray(control.value) && control.value.indexOf(text) !== -1

            onClicked:
                buttons.updateValue()
        }

        function updateValue()
        {
            var newValue = []
            for (var i = 0; i != count; ++i)
            {
                var item = itemAt(i)
                if (item.checked)
                    newValue.push(item.text)
            }

            control.value = newValue
        }
    }
}
