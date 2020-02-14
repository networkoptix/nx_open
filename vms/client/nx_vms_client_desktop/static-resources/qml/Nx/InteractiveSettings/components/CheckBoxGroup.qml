import QtQuick 2.11

import Nx.Controls 1.0

import "private"

LabeledItem
{
    id: control

    isGroup: buttons.count > 1

    // TODO: #dfisenko: rename to "defaultTooltip" for enabling tooltips
    property var defaultTooltip_: ""
    property var value: defaultValue
    property var defaultValue
    property var itemCaptions
    property alias range: buttons.model

    contentItem: MultiColumn
    {
        id: buttons

        baselineOffset: (count > 0) ? itemAt(0).baselineOffset : 0

        delegate: CheckBox
        {
            readonly property var identifier: modelData

            text: (control.itemCaptions && control.itemCaptions[identifier]) || identifier
            checked: Array.isArray(control.value) && control.value.indexOf(identifier) !== -1

            onClicked:
                buttons.updateValue()

            onHoveredChanged:
            {
                if (hovered)
                    control.defaultTooltip_ = defaultValue.indexOf(identifier) !== -1
            }
        }

        function updateValue()
        {
            var newValue = []
            for (var i = 0; i != count; ++i)
            {
                var item = itemAt(i)
                if (item.checked)
                    newValue.push(item.identifier)
            }

            control.value = newValue
        }
    }
}
