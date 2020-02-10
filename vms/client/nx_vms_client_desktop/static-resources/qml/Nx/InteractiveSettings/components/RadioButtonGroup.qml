import QtQuick 2.11

import Nx.Controls 1.0

import "private"

LabeledItem
{
    id: control

    isGroup: buttons.count > 1

    property var value: defaultValue
    property var defaultValue
    property var itemCaptions
    property alias range: buttons.model

    contentItem: MultiColumn
    {
        id: buttons

        baselineOffset: (count > 0) ? itemAt(0).baselineOffset : 0

        delegate: RadioButton
        {
            readonly property var identifier: modelData

            text: (control.itemCaptions && control.itemCaptions[identifier]) || identifier
            checked: control.value == identifier

            onClicked:
                control.value = identifier
        }
    }
}
